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

#include "cfg.h"
#include "codegen-inl.h"
#include "codegen-x64.h"
#include "debug.h"
#include "macro-assembler-x64.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void InstructionBlock::Compile(MacroAssembler* masm) {
  ASSERT(!is_marked());
  is_marked_ = true;
  {
    Comment cmt(masm, "[ InstructionBlock");
    for (int i = 0, len = instructions_.length(); i < len; i++) {
      // If the location of the current instruction is a temp, then the
      // instruction cannot be in tail position in the block.  Allocate the
      // temp based on peeking ahead to the next instruction.
      Instruction* instr = instructions_[i];
      Location* loc = instr->location();
      if (loc->is_temporary()) {
        instructions_[i+1]->FastAllocate(TempLocation::cast(loc));
      }
      instructions_[i]->Compile(masm);
    }
  }
  successor_->Compile(masm);
}


void EntryNode::Compile(MacroAssembler* masm) {
  ASSERT(!is_marked());
  is_marked_ = true;
  Label deferred_enter, deferred_exit;
  {
    Comment cmnt(masm, "[ EntryNode");
    __ push(rbp);
    __ movq(rbp, rsp);
    __ push(rsi);
    __ push(rdi);
    int count = CfgGlobals::current()->fun()->scope()->num_stack_slots();
    if (count > 0) {
      __ movq(kScratchRegister, Factory::undefined_value(),
              RelocInfo::EMBEDDED_OBJECT);
      for (int i = 0; i < count; i++) {
        __ push(kScratchRegister);
      }
    }
    if (FLAG_trace) {
      __ CallRuntime(Runtime::kTraceEnter, 0);
    }
    if (FLAG_check_stack) {
      ExternalReference stack_limit =
          ExternalReference::address_of_stack_guard_limit();
      __ movq(kScratchRegister, stack_limit);
      __ cmpq(rsp, Operand(kScratchRegister, 0));
      __ j(below, &deferred_enter);
      __ bind(&deferred_exit);
    }
  }
  successor_->Compile(masm);
  if (FLAG_check_stack) {
    Comment cmnt(masm, "[ Deferred Stack Check");
    __ bind(&deferred_enter);
    StackCheckStub stub;
    __ CallStub(&stub);
    __ jmp(&deferred_exit);
  }
}


void ExitNode::Compile(MacroAssembler* masm) {
  ASSERT(!is_marked());
  is_marked_ = true;

  Comment cmnt(masm, "[ ExitNode");
  if (FLAG_trace) {
    __ push(rax);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }
  __ RecordJSReturn();
  __ movq(rsp, rbp);
  __ pop(rbp);
  int count = CfgGlobals::current()->fun()->scope()->num_parameters();
  __ ret((count + 1) * kPointerSize);
  // Add padding that will be overwritten by a debugger breakpoint.
  // "movq rsp, rbp; pop rbp" has length 5.  "ret k" has length 2.
  const int kPadding = Debug::kX64JSReturnSequenceLength - 5 - 2;
  for (int i = 0; i < kPadding; ++i) {
    __ int3();
  }
}


void PositionInstr::Compile(MacroAssembler* masm) {
  if (FLAG_debug_info && pos_ != RelocInfo::kNoPosition) {
    __ RecordStatementPosition(pos_);
    __ RecordPosition(pos_);
  }
}


void BinaryOpInstr::Compile(MacroAssembler* masm) {
  // The right-hand value should not be on the stack---if it is a
  // compiler-generated temporary it is in the accumulator.
  ASSERT(!val1_->is_on_stack());

  Comment cmnt(masm, "[ BinaryOpInstr");
  // We can overwrite one of the operands if it is a temporary.
  OverwriteMode mode = NO_OVERWRITE;
  if (val0_->is_temporary()) {
    mode = OVERWRITE_LEFT;
  } else if (val1_->is_temporary()) {
    mode = OVERWRITE_RIGHT;
  }

  // Push both operands and call the specialized stub.
  if (!val0_->is_on_stack()) {
    val0_->Push(masm);
  }
  val1_->Push(masm);
  GenericBinaryOpStub stub(op_, mode, SMI_CODE_IN_STUB);
  __ CallStub(&stub);
  loc_->Set(masm, rax);
}


void ReturnInstr::Compile(MacroAssembler* masm) {
  // The location should be 'Effect'.  As a side effect, move the value to
  // the accumulator.
  Comment cmnt(masm, "[ ReturnInstr");
  value_->Get(masm, rax);
}


void Constant::Get(MacroAssembler* masm, Register reg) {
  __ Move(reg, handle_);
}


void Constant::Push(MacroAssembler* masm) {
  __ Push(handle_);
}


static Operand ToOperand(SlotLocation* loc) {
  switch (loc->type()) {
    case Slot::PARAMETER: {
      int count = CfgGlobals::current()->fun()->scope()->num_parameters();
      return Operand(rbp, (1 + count - loc->index()) * kPointerSize);
    }
    case Slot::LOCAL: {
      const int kOffset = JavaScriptFrameConstants::kLocal0Offset;
      return Operand(rbp, kOffset - loc->index() * kPointerSize);
    }
    default:
      UNREACHABLE();
      return Operand(rax, 0);
  }
}


void SlotLocation::Get(MacroAssembler* masm, Register reg) {
  __ movq(reg, ToOperand(this));
}


void SlotLocation::Set(MacroAssembler* masm, Register reg) {
  __ movq(ToOperand(this), reg);
}


void SlotLocation::Push(MacroAssembler* masm) {
  __ push(ToOperand(this));
}


void TempLocation::Get(MacroAssembler* masm, Register reg) {
  switch (where_) {
    case ACCUMULATOR:
      if (!reg.is(rax)) __ movq(reg, rax);
      break;
    case STACK:
      __ pop(reg);
      break;
    case NOWHERE:
      UNREACHABLE();
      break;
  }
}


void TempLocation::Set(MacroAssembler* masm, Register reg) {
  switch (where_) {
    case ACCUMULATOR:
      if (!reg.is(rax)) __ movq(rax, reg);
      break;
    case STACK:
      __ push(reg);
      break;
    case NOWHERE:
      UNREACHABLE();
      break;
  }
}


void TempLocation::Push(MacroAssembler* masm) {
  switch (where_) {
    case ACCUMULATOR:
      __ push(rax);
      break;
    case STACK:
    case NOWHERE:
      UNREACHABLE();
      break;
  }
}


#undef __

} }  // namespace v8::internal
