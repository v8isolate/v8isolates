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
#include "macro-assembler-ia32.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void InstructionBlock::Compile(MacroAssembler* masm) {
  ASSERT(!is_marked());
  is_marked_ = true;
  {
    Comment cmt(masm, "[ InstructionBlock");
    for (int i = 0, len = instructions_.length(); i < len; i++) {
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
    __ push(ebp);
    __ mov(ebp, esp);
    __ push(esi);
    __ push(edi);
    if (local_count_ > 0) {
      __ Set(eax, Immediate(Factory::undefined_value()));
      for (int i = 0; i < local_count_; i++) {
        __ push(eax);
      }
    }
    if (FLAG_check_stack) {
      ExternalReference stack_limit =
          ExternalReference::address_of_stack_guard_limit();
      __ cmp(esp, Operand::StaticVariable(stack_limit));
      __ j(below, &deferred_enter);
      __ bind(&deferred_exit);
    }
  }
  successor_->Compile(masm);
  if (FLAG_check_stack) {
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
  __ RecordJSReturn();
  __ mov(esp, ebp);
  __ pop(ebp);
  __ ret((parameter_count_ + 1) * kPointerSize);
}


void ReturnInstr::Compile(MacroAssembler* masm) {
  Comment cmnt(masm, "[ ReturnInstr");
  value_->ToRegister(masm, eax);
}


void Constant::ToRegister(MacroAssembler* masm, Register reg) {
  __ mov(reg, Immediate(handle_));
}

#undef __

} }  // namespace v8::internal
