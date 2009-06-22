// Copyright 2009 the V8 project authors. All rights reserved.
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

#include "codegen-inl.h"
#include "register-allocator-inl.h"
#include "scopes.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

// -------------------------------------------------------------------------
// VirtualFrame implementation.

// On entry to a function, the virtual frame already contains the receiver,
// the parameters, and a return address.  All frame elements are in memory.
VirtualFrame::VirtualFrame()
    : elements_(parameter_count() + local_count() + kPreallocatedElements),
      stack_pointer_(parameter_count() + 1) {  // 0-based index of TOS.
  for (int i = 0; i <= stack_pointer_; i++) {
    elements_.Add(FrameElement::MemoryElement());
  }
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    register_locations_[i] = kIllegalIndex;
  }
}


void VirtualFrame::Enter() {
  // Registers live on entry to a JS frame:
  //   rsp: stack pointer, points to return address from this function.
  //   rbp: base pointer, points to previous JS, ArgumentsAdaptor, or
  //        Trampoline frame.
  //   rsi: context of this function call.
  //   rdi: pointer to this function object.
  Comment cmnt(masm(), "[ Enter JS frame");

#ifdef DEBUG
  // Verify that rdi contains a JS function.  The following code
  // relies on rax being available for use.
  __ testq(rdi, Immediate(kSmiTagMask));
  __ Check(not_zero,
           "VirtualFrame::Enter - rdi is not a function (smi check).");
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rax);
  __ Check(equal,
           "VirtualFrame::Enter - rdi is not a function (map check).");
#endif

  EmitPush(rbp);

  __ movq(rbp, rsp);

  // Store the context in the frame.  The context is kept in rsi and a
  // copy is stored in the frame.  The external reference to rsi
  // remains.
  EmitPush(rsi);

  // Store the function in the frame.  The frame owns the register
  // reference now (ie, it can keep it in rdi or spill it later).
  Push(rdi);
  SyncElementAt(element_count() - 1);
  cgen()->allocator()->Unuse(rdi);
}


void VirtualFrame::Exit() {
  Comment cmnt(masm(), "[ Exit JS frame");
  // Record the location of the JS exit code for patching when setting
  // break point.
  __ RecordJSReturn();

  // Avoid using the leave instruction here, because it is too
  // short. We need the return sequence to be a least the size of a
  // call instruction to support patching the exit code in the
  // debugger. See GenerateReturnSequence for the full return sequence.
  // TODO(X64): A patched call will be very long now.  Make sure we
  // have enough room.
  __ movq(rsp, rbp);
  stack_pointer_ = frame_pointer();
  for (int i = element_count() - 1; i > stack_pointer_; i--) {
    FrameElement last = elements_.RemoveLast();
    if (last.is_register()) {
      Unuse(last.reg());
    }
  }

  EmitPop(rbp);
}


void VirtualFrame::AllocateStackSlots() {
  int count = local_count();
  if (count > 0) {
    Comment cmnt(masm(), "[ Allocate space for locals");
    // The locals are initialized to a constant (the undefined value), but
    // we sync them with the actual frame to allocate space for spilling
    // them later.  First sync everything above the stack pointer so we can
    // use pushes to allocate and initialize the locals.
    SyncRange(stack_pointer_ + 1, element_count() - 1);
    Handle<Object> undefined = Factory::undefined_value();
    FrameElement initial_value =
        FrameElement::ConstantElement(undefined, FrameElement::SYNCED);
    __ movq(kScratchRegister, undefined, RelocInfo::EMBEDDED_OBJECT);
    for (int i = 0; i < count; i++) {
      elements_.Add(initial_value);
      stack_pointer_++;
      __ push(kScratchRegister);
    }
  }
}


void VirtualFrame::SaveContextRegister() {
  ASSERT(elements_[context_index()].is_memory());
  __ movq(Operand(rbp, fp_relative(context_index())), rsi);
}


void VirtualFrame::RestoreContextRegister() {
  ASSERT(elements_[context_index()].is_memory());
  __ movq(rsi, Operand(rbp, fp_relative(context_index())));
}


void VirtualFrame::PushReceiverSlotAddress() {
  Result temp = cgen()->allocator()->Allocate();
  ASSERT(temp.is_valid());
  __ lea(temp.reg(), ParameterAt(-1));
  Push(&temp);
}


void VirtualFrame::EmitPop(Register reg) {
  ASSERT(stack_pointer_ == element_count() - 1);
  stack_pointer_--;
  elements_.RemoveLast();
  __ pop(reg);
}


void VirtualFrame::EmitPop(const Operand& operand) {
  ASSERT(stack_pointer_ == element_count() - 1);
  stack_pointer_--;
  elements_.RemoveLast();
  __ pop(operand);
}


void VirtualFrame::EmitPush(Register reg) {
  ASSERT(stack_pointer_ == element_count() - 1);
  elements_.Add(FrameElement::MemoryElement());
  stack_pointer_++;
  __ push(reg);
}


void VirtualFrame::EmitPush(const Operand& operand) {
  ASSERT(stack_pointer_ == element_count() - 1);
  elements_.Add(FrameElement::MemoryElement());
  stack_pointer_++;
  __ push(operand);
}


void VirtualFrame::EmitPush(Immediate immediate) {
  ASSERT(stack_pointer_ == element_count() - 1);
  elements_.Add(FrameElement::MemoryElement());
  stack_pointer_++;
  __ push(immediate);
}


void VirtualFrame::Drop(int count) {
  ASSERT(height() >= count);
  int num_virtual_elements = (element_count() - 1) - stack_pointer_;

  // Emit code to lower the stack pointer if necessary.
  if (num_virtual_elements < count) {
    int num_dropped = count - num_virtual_elements;
    stack_pointer_ -= num_dropped;
    __ addq(rsp, Immediate(num_dropped * kPointerSize));
  }

  // Discard elements from the virtual frame and free any registers.
  for (int i = 0; i < count; i++) {
    FrameElement dropped = elements_.RemoveLast();
    if (dropped.is_register()) {
      Unuse(dropped.reg());
    }
  }
}


int VirtualFrame::InvalidateFrameSlotAt(int index) {
  FrameElement original = elements_[index];

  // Is this element the backing store of any copies?
  int new_backing_index = kIllegalIndex;
  if (original.is_copied()) {
    // Verify it is copied, and find first copy.
    for (int i = index + 1; i < element_count(); i++) {
      if (elements_[i].is_copy() && elements_[i].index() == index) {
        new_backing_index = i;
        break;
      }
    }
  }

  if (new_backing_index == kIllegalIndex) {
    // No copies found, return kIllegalIndex.
    if (original.is_register()) {
      Unuse(original.reg());
    }
    elements_[index] = FrameElement::InvalidElement();
    return kIllegalIndex;
  }

  // This is the backing store of copies.
  Register backing_reg;
  if (original.is_memory()) {
    Result fresh = cgen()->allocator()->Allocate();
    ASSERT(fresh.is_valid());
    Use(fresh.reg(), new_backing_index);
    backing_reg = fresh.reg();
    __ movq(backing_reg, Operand(rbp, fp_relative(index)));
  } else {
    // The original was in a register.
    backing_reg = original.reg();
    set_register_location(backing_reg, new_backing_index);
  }
  // Invalidate the element at index.
  elements_[index] = FrameElement::InvalidElement();
  // Set the new backing element.
  if (elements_[new_backing_index].is_synced()) {
    elements_[new_backing_index] =
        FrameElement::RegisterElement(backing_reg, FrameElement::SYNCED);
  } else {
    elements_[new_backing_index] =
        FrameElement::RegisterElement(backing_reg, FrameElement::NOT_SYNCED);
  }
  // Update the other copies.
  for (int i = new_backing_index + 1; i < element_count(); i++) {
    if (elements_[i].is_copy() && elements_[i].index() == index) {
      elements_[i].set_index(new_backing_index);
      elements_[new_backing_index].set_copied();
    }
  }
  return new_backing_index;
}


void VirtualFrame::StoreToFrameSlotAt(int index) {
  // Store the value on top of the frame to the virtual frame slot at
  // a given index.  The value on top of the frame is left in place.
  // This is a duplicating operation, so it can create copies.
  ASSERT(index >= 0);
  ASSERT(index < element_count());

  int top_index = element_count() - 1;
  FrameElement top = elements_[top_index];
  FrameElement original = elements_[index];
  if (top.is_copy() && top.index() == index) return;
  ASSERT(top.is_valid());

  InvalidateFrameSlotAt(index);

  // InvalidateFrameSlotAt can potentially change any frame element, due
  // to spilling registers to allocate temporaries in order to preserve
  // the copy-on-write semantics of aliased elements.  Reload top from
  // the frame.
  top = elements_[top_index];

  if (top.is_copy()) {
    // There are two cases based on the relative positions of the
    // stored-to slot and the backing slot of the top element.
    int backing_index = top.index();
    ASSERT(backing_index != index);
    if (backing_index < index) {
      // 1. The top element is a copy of a slot below the stored-to
      // slot.  The stored-to slot becomes an unsynced copy of that
      // same backing slot.
      elements_[index] = CopyElementAt(backing_index);
    } else {
      // 2. The top element is a copy of a slot above the stored-to
      // slot.  The stored-to slot becomes the new (unsynced) backing
      // slot and both the top element and the element at the former
      // backing slot become copies of it.  The sync state of the top
      // and former backing elements is preserved.
      FrameElement backing_element = elements_[backing_index];
      ASSERT(backing_element.is_memory() || backing_element.is_register());
      if (backing_element.is_memory()) {
        // Because sets of copies are canonicalized to be backed by
        // their lowest frame element, and because memory frame
        // elements are backed by the corresponding stack address, we
        // have to move the actual value down in the stack.
        //
        // TODO(209): considering allocating the stored-to slot to the
        // temp register.  Alternatively, allow copies to appear in
        // any order in the frame and lazily move the value down to
        // the slot.
        __ movq(kScratchRegister, Operand(rbp, fp_relative(backing_index)));
        __ movq(Operand(rbp, fp_relative(index)), kScratchRegister);
      } else {
        set_register_location(backing_element.reg(), index);
        if (backing_element.is_synced()) {
          // If the element is a register, we will not actually move
          // anything on the stack but only update the virtual frame
          // element.
          backing_element.clear_sync();
        }
      }
      elements_[index] = backing_element;

      // The old backing element becomes a copy of the new backing
      // element.
      FrameElement new_element = CopyElementAt(index);
      elements_[backing_index] = new_element;
      if (backing_element.is_synced()) {
        elements_[backing_index].set_sync();
      }

      // All the copies of the old backing element (including the top
      // element) become copies of the new backing element.
      for (int i = backing_index + 1; i < element_count(); i++) {
        if (elements_[i].is_copy() && elements_[i].index() == backing_index) {
          elements_[i].set_index(index);
        }
      }
    }
    return;
  }

  // Move the top element to the stored-to slot and replace it (the
  // top element) with a copy.
  elements_[index] = top;
  if (top.is_memory()) {
    // TODO(209): consider allocating the stored-to slot to the temp
    // register.  Alternatively, allow copies to appear in any order
    // in the frame and lazily move the value down to the slot.
    FrameElement new_top = CopyElementAt(index);
    new_top.set_sync();
    elements_[top_index] = new_top;

    // The sync state of the former top element is correct (synced).
    // Emit code to move the value down in the frame.
    __ movq(kScratchRegister, Operand(rsp, 0));
    __ movq(Operand(rbp, fp_relative(index)), kScratchRegister);
  } else if (top.is_register()) {
    set_register_location(top.reg(), index);
    // The stored-to slot has the (unsynced) register reference and
    // the top element becomes a copy.  The sync state of the top is
    // preserved.
    FrameElement new_top = CopyElementAt(index);
    if (top.is_synced()) {
      new_top.set_sync();
      elements_[index].clear_sync();
    }
    elements_[top_index] = new_top;
  } else {
    // The stored-to slot holds the same value as the top but
    // unsynced.  (We do not have copies of constants yet.)
    ASSERT(top.is_constant());
    elements_[index].clear_sync();
  }
}


void VirtualFrame::MakeMergable() {
  // UNIMPLEMENTED();
}

void VirtualFrame::MergeTo(VirtualFrame* a) {
  UNIMPLEMENTED();
}


Result VirtualFrame::Pop() {
  FrameElement element = elements_.RemoveLast();
  int index = element_count();
  ASSERT(element.is_valid());

  bool pop_needed = (stack_pointer_ == index);
  if (pop_needed) {
    stack_pointer_--;
    if (element.is_memory()) {
      Result temp = cgen()->allocator()->Allocate();
      ASSERT(temp.is_valid());
      temp.set_static_type(element.static_type());
      __ pop(temp.reg());
      return temp;
    }

    __ addq(rsp, Immediate(kPointerSize));
  }
  ASSERT(!element.is_memory());

  // The top element is a register, constant, or a copy.  Unuse
  // registers and follow copies to their backing store.
  if (element.is_register()) {
    Unuse(element.reg());
  } else if (element.is_copy()) {
    ASSERT(element.index() < index);
    index = element.index();
    element = elements_[index];
  }
  ASSERT(!element.is_copy());

  // The element is memory, a register, or a constant.
  if (element.is_memory()) {
    // Memory elements could only be the backing store of a copy.
    // Allocate the original to a register.
    ASSERT(index <= stack_pointer_);
    Result temp = cgen()->allocator()->Allocate();
    ASSERT(temp.is_valid());
    Use(temp.reg(), index);
    FrameElement new_element =
        FrameElement::RegisterElement(temp.reg(), FrameElement::SYNCED);
    // Preserve the copy flag on the element.
    if (element.is_copied()) new_element.set_copied();
    new_element.set_static_type(element.static_type());
    elements_[index] = new_element;
    __ movq(temp.reg(), Operand(rbp, fp_relative(index)));
    return Result(temp.reg(), element.static_type());
  } else if (element.is_register()) {
    return Result(element.reg(), element.static_type());
  } else {
    ASSERT(element.is_constant());
    return Result(element.handle());
  }
}


Result VirtualFrame::RawCallStub(CodeStub* stub) {
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallStub(stub);
  Result result = cgen()->allocator()->Allocate(rax);
  ASSERT(result.is_valid());
  return result;
}


void VirtualFrame::SyncElementBelowStackPointer(int index) {
  // Emit code to write elements below the stack pointer to their
  // (already allocated) stack address.
  ASSERT(index <= stack_pointer_);
  FrameElement element = elements_[index];
  ASSERT(!element.is_synced());
  switch (element.type()) {
    case FrameElement::INVALID:
      break;

    case FrameElement::MEMORY:
      // This function should not be called with synced elements.
      // (memory elements are always synced).
      UNREACHABLE();
      break;

    case FrameElement::REGISTER:
      __ movq(Operand(rbp, fp_relative(index)), element.reg());
      break;

    case FrameElement::CONSTANT:
      if (element.handle()->IsSmi()) {
        if (CodeGeneratorScope::Current()->IsUnsafeSmi(element.handle())) {
          CodeGeneratorScope::Current()->LoadUnsafeSmi(kScratchRegister,
                                                       element.handle());
        } else {
          __ movq(kScratchRegister, element.handle(), RelocInfo::NONE);
        }
      } else {
        __ movq(kScratchRegister,
                 element.handle(),
                 RelocInfo::EMBEDDED_OBJECT);
      }
      __ movq(Operand(rbp, fp_relative(index)), kScratchRegister);
      break;

    case FrameElement::COPY: {
      int backing_index = element.index();
      FrameElement backing_element = elements_[backing_index];
      if (backing_element.is_memory()) {
        __ movq(kScratchRegister, Operand(rbp, fp_relative(backing_index)));
        __ movq(Operand(rbp, fp_relative(index)), kScratchRegister);
      } else {
        ASSERT(backing_element.is_register());
        __ movq(Operand(rbp, fp_relative(index)), backing_element.reg());
      }
      break;
    }
  }
  elements_[index].set_sync();
}


void VirtualFrame::SyncElementByPushing(int index) {
  // Sync an element of the frame that is just above the stack pointer
  // by pushing it.
  ASSERT(index == stack_pointer_ + 1);
  stack_pointer_++;
  FrameElement element = elements_[index];

  switch (element.type()) {
    case FrameElement::INVALID:
      __ push(Immediate(Smi::FromInt(0)));
      break;

    case FrameElement::MEMORY:
      // No memory elements exist above the stack pointer.
      UNREACHABLE();
      break;

    case FrameElement::REGISTER:
      __ push(element.reg());
      break;

    case FrameElement::CONSTANT:
      if (element.handle()->IsSmi()) {
        if (CodeGeneratorScope::Current()->IsUnsafeSmi(element.handle())) {
          CodeGeneratorScope::Current()->LoadUnsafeSmi(kScratchRegister,
                                                       element.handle());
        } else {
          CodeGeneratorScope::Current()->masm()->
              movq(kScratchRegister, element.handle(), RelocInfo::NONE);
        }
      } else {
        CodeGeneratorScope::Current()->masm()->
            movq(kScratchRegister,
                 element.handle(),
                 RelocInfo::EMBEDDED_OBJECT);
      }
      __ push(kScratchRegister);
      break;

    case FrameElement::COPY: {
      int backing_index = element.index();
      FrameElement backing = elements_[backing_index];
      ASSERT(backing.is_memory() || backing.is_register());
      if (backing.is_memory()) {
        __ push(Operand(rbp, fp_relative(backing_index)));
      } else {
        __ push(backing.reg());
      }
      break;
    }
  }
  elements_[index].set_sync();
}


// Clear the dirty bits for the range of elements in
// [min(stack_pointer_ + 1,begin), end].
void VirtualFrame::SyncRange(int begin, int end) {
  ASSERT(begin >= 0);
  ASSERT(end < element_count());
  // Sync elements below the range if they have not been materialized
  // on the stack.
  int start = Min(begin, stack_pointer_ + 1);

  // If positive we have to adjust the stack pointer.
  int delta = end - stack_pointer_;
  if (delta > 0) {
    stack_pointer_ = end;
    __ subq(rsp, Immediate(delta * kPointerSize));
  }

  for (int i = start; i <= end; i++) {
    if (!elements_[i].is_synced()) SyncElementBelowStackPointer(i);
  }
}

//------------------------------------------------------------------------------
// Virtual frame stub and IC calling functions.

Result VirtualFrame::RawCallCodeObject(Handle<Code> code,
                                       RelocInfo::Mode rmode) {
  ASSERT(cgen()->HasValidEntryRegisters());
  __ Call(code, rmode);
  Result result = cgen()->allocator()->Allocate(rax);
  ASSERT(result.is_valid());
  return result;
}


Result VirtualFrame::CallRuntime(Runtime::Function* f, int arg_count) {
  PrepareForCall(arg_count, arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallRuntime(f, arg_count);
  Result result = cgen()->allocator()->Allocate(rax);
  ASSERT(result.is_valid());
  return result;
}


Result VirtualFrame::CallRuntime(Runtime::FunctionId id, int arg_count) {
  PrepareForCall(arg_count, arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallRuntime(id, arg_count);
  Result result = cgen()->allocator()->Allocate(rax);
  ASSERT(result.is_valid());
  return result;
}


Result VirtualFrame::CallCallIC(RelocInfo::Mode mode,
                                int arg_count,
                                int loop_nesting) {
  // Arguments, receiver, and function name are on top of the frame.
  // The IC expects them on the stack.  It does not drop the function
  // name slot (but it does drop the rest).
  InLoopFlag in_loop = loop_nesting > 0 ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = cgen()->ComputeCallInitialize(arg_count, in_loop);
  // Spill args, receiver, and function.  The call will drop args and
  // receiver.
  PrepareForCall(arg_count + 2, arg_count + 1);
  return RawCallCodeObject(ic, mode);
}




#undef __

} }  // namespace v8::internal
