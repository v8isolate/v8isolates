// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "codegen.h"
#include "jump-target.h"

namespace v8 { namespace internal {

// -------------------------------------------------------------------------
// JumpTarget implementation.

#define __ masm_->

void JumpTarget::Jump() {
  ASSERT(cgen_ != NULL);
  ASSERT(cgen_->has_valid_frame());
  // Live non-frame registers are not allowed at unconditional jumps
  // because we have no way of invalidating the corresponding results
  // which are still live in the C++ code.
  ASSERT(cgen_->HasValidEntryRegisters());

  if (is_bound()) {
    // Backward jump.  There is an expected frame to merge to.
    ASSERT(direction_ == BIDIRECTIONAL);
    cgen_->frame()->MergeTo(entry_frame_);
    cgen_->DeleteFrame();
    __ jmp(&entry_label_);
  } else {
    // Forward jump.  The current frame is added to the end of the list
    // of frames reaching the target block and a jump to the merge code
    // is emitted.
    AddReachingFrame(cgen_->frame());
    RegisterFile empty;
    cgen_->SetFrame(NULL, &empty);
    __ jmp(&merge_labels_.last());
  }

  is_linked_ = !is_bound_;
}


void JumpTarget::Branch(Condition cc, Hint ignored) {
  ASSERT(cgen_ != NULL);
  ASSERT(cgen_->has_valid_frame());

  if (is_bound()) {
    // Backward branch.  We have an expected frame to merge to on the
    // backward edge.  We negate the condition and emit the merge code
    // here.
    //
    // TODO(210): we should try to avoid negating the condition in the
    // case where there is no merge code to emit.  Otherwise, we emit
    // a branch around an unconditional jump.
    ASSERT(direction_ == BIDIRECTIONAL);
    Label original_fall_through;
    __ b(NegateCondition(cc), &original_fall_through);
    // Swap the current frame for a copy of it, saving non-frame
    // register reference counts and invalidating all non-frame register
    // references except the reserved ones on the backward edge.
    VirtualFrame* original_frame = cgen_->frame();
    VirtualFrame* working_frame = new VirtualFrame(original_frame);
    RegisterFile non_frame_registers = RegisterAllocator::Reserved();
    cgen_->SetFrame(working_frame, &non_frame_registers);

    working_frame->MergeTo(entry_frame_);
    cgen_->DeleteFrame();
    __ jmp(&entry_label_);

    // Restore the frame and its associated non-frame registers.
    cgen_->SetFrame(original_frame, &non_frame_registers);
    __ bind(&original_fall_through);
  } else {
    // Forward branch.  A copy of the current frame is added to the end
    // of the list of frames reaching the target block and a branch to
    // the merge code is emitted.
    AddReachingFrame(new VirtualFrame(cgen_->frame()));
    __ b(cc, &merge_labels_.last());
  }

  is_linked_ = !is_bound_;
}


void JumpTarget::Call() {
  // Call is used to push the address of the catch block on the stack as
  // a return address when compiling try/catch and try/finally.  We
  // fully spill the frame before making the call.  The expected frame
  // at the label (which should be the only one) is the spilled current
  // frame plus an in-memory return address.  The "fall-through" frame
  // at the return site is the spilled current frame.
  ASSERT(cgen_ != NULL);
  ASSERT(cgen_->has_valid_frame());
  // There are no non-frame references across the call.
  ASSERT(cgen_->HasValidEntryRegisters());
  ASSERT(!is_linked());

  cgen_->frame()->SpillAll();
  VirtualFrame* target_frame = new VirtualFrame(cgen_->frame());
  target_frame->Adjust(1);
  AddReachingFrame(target_frame);
  __ bl(&merge_labels_.last());

  is_linked_ = !is_bound_;
}


void JumpTarget::Bind(int mergable_elements) {
  ASSERT(cgen_ != NULL);
  ASSERT(!is_bound());

  // Live non-frame registers are not allowed at the start of a basic
  // block.
  ASSERT(!cgen_->has_valid_frame() || cgen_->HasValidEntryRegisters());

  // Compute the frame to use for entry to the block.
  ComputeEntryFrame(mergable_elements);

  if (is_linked()) {
    // There were forward jumps.  Handle merging the reaching frames
    // and possible fall through to the entry frame.

    // Some moves required to merge to an expected frame require
    // purely frame state changes, and do not require any code
    // generation.  Perform those first to increase the possibility of
    // finding equal frames below.
    if (cgen_->has_valid_frame()) {
      cgen_->frame()->PrepareMergeTo(entry_frame_);
    }
    for (int i = 0; i < reaching_frames_.length(); i++) {
      reaching_frames_[i]->PrepareMergeTo(entry_frame_);
    }

    // If there is a fall through to the jump target and it needs
    // merge code, process it first.
    if (cgen_->has_valid_frame() && !cgen_->frame()->Equals(entry_frame_)) {
      // Loop over all the reaching frames, looking for any that can
      // share merge code with this one.
      for (int i = 0; i < reaching_frames_.length(); i++) {
        if (cgen_->frame()->Equals(reaching_frames_[i])) {
          // Set the reaching frames element to null to avoid
          // processing it later, and then bind its entry label.
          delete reaching_frames_[i];
          reaching_frames_[i] = NULL;
          __ bind(&merge_labels_[i]);
        }
      }

      // Emit the merge code.
      cgen_->frame()->MergeTo(entry_frame_);
    }

    // Loop over the (non-null) reaching frames and process any that
    // need merge code.
    for (int i = 0; i < reaching_frames_.length(); i++) {
      VirtualFrame* frame = reaching_frames_[i];
      if (frame != NULL && !frame->Equals(entry_frame_)) {
        // Set the reaching frames element to null to avoid processing
        // it later.  Do not delete it as it is needed for merging.
        reaching_frames_[i] = NULL;

        // If the code generator has a current frame (a fall-through
        // or a previously merged frame), insert a jump around the
        // merge code we are about to generate.
        if (cgen_->has_valid_frame()) {
          cgen_->DeleteFrame();
          __ jmp(&entry_label_);
        }

        // Set the frame to merge as the code generator's current
        // frame and bind its merge label.
        RegisterFile reserved_registers = RegisterAllocator::Reserved();
        cgen_->SetFrame(frame, &reserved_registers);
        __ bind(&merge_labels_[i]);

        // Loop over the remaining (non-null) reaching frames, looking
        // for any that can share merge code with this one.
        for (int j = i + 1; j < reaching_frames_.length(); j++) {
          VirtualFrame* other = reaching_frames_[j];
          if (other != NULL && frame->Equals(other)) {
            delete other;
            reaching_frames_[j] = NULL;
            __ bind(&merge_labels_[j]);
          }
        }

        // Emit the merge code.
        cgen_->frame()->MergeTo(entry_frame_);
      }
    }

    // The code generator may not have a current frame if there was no
    // fall through and none of the reaching frames needed merging.
    // In that case, clone the entry frame as the current frame.
    if (!cgen_->has_valid_frame()) {
      RegisterFile reserved_registers = RegisterAllocator::Reserved();
      cgen_->SetFrame(new VirtualFrame(entry_frame_), &reserved_registers);
    }

    // There is certainly a current frame equal to the entry frame.
    // Bind the entry frame label.
    __ bind(&entry_label_);

    // There may be unprocessed reaching frames that did not need
    // merge code.  Bind their merge labels to be the same as the
    // entry label.
    for (int i = 0; i < reaching_frames_.length(); i++) {
      if (reaching_frames_[i] != NULL) {
        delete reaching_frames_[i];
        __ bind(&merge_labels_[i]);
      }
    }

    // All the reaching frames except the one that is the current
    // frame (if it is one of the reaching frames) have been deleted.
    reaching_frames_.Clear();
    merge_labels_.Clear();

  } else {
    // There were no forward jumps.  The current frame is merged to
    // the entry frame.
    cgen_->frame()->MergeTo(entry_frame_);
    __ bind(&entry_label_);
  }

  is_linked_ = false;
  is_bound_ = true;
}

#undef __


} }  // namespace v8::internal
