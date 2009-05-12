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

#ifndef V8_X64_ASSEMBLER_X64_INL_H_
#define V8_X64_ASSEMBLER_X64_INL_H_

#include "cpu.h"

namespace v8 { namespace internal {

Condition NegateCondition(Condition cc) {
  return static_cast<Condition>(cc ^ 1);
}


// The modes possibly affected by apply must be in kApplyMask.
void RelocInfo::apply(int delta) {
  if (rmode_ == RUNTIME_ENTRY || IsCodeTarget(rmode_)) {
    intptr_t* p = reinterpret_cast<intptr_t*>(pc_);
    *p -= delta;  // relocate entry
  } else if (rmode_ == JS_RETURN && IsCallInstruction()) {
    // Special handling of js_return when a break point is set (call
    // instruction has been inserted).
    intptr_t* p = reinterpret_cast<intptr_t*>(pc_ + 1);
    *p -= delta;  // relocate entry
  } else if (IsInternalReference(rmode_)) {
    // absolute code pointer inside code object moves with the code object.
    intptr_t* p = reinterpret_cast<intptr_t*>(pc_);
    *p += delta;  // relocate entry
  }
}


Address RelocInfo::target_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  return Assembler::target_address_at(pc_);
}


Address RelocInfo::target_address_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  return reinterpret_cast<Address>(pc_);
}


void RelocInfo::set_target_address(Address target) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  Assembler::set_target_address_at(pc_, target);
}


void Assembler::set_target_address_at(byte* location, byte* value) {
  UNIMPLEMENTED();
}


byte* Assembler::target_address_at(byte* location) {
  UNIMPLEMENTED();
  return NULL;
}


Object* RelocInfo::target_object() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return *reinterpret_cast<Object**>(pc_);
}


Object** RelocInfo::target_object_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object**>(pc_);
}


Address* RelocInfo::target_reference_address() {
  ASSERT(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  return reinterpret_cast<Address*>(pc_);
}


void RelocInfo::set_target_object(Object* target) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  *reinterpret_cast<Object**>(pc_) = target;
}


bool RelocInfo::IsCallInstruction() {
  UNIMPLEMENTED();  // IA32 code below.
  return *pc_ == 0xE8;
}


Address RelocInfo::call_address() {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  return Assembler::target_address_at(pc_ + 1);
}


void RelocInfo::set_call_address(Address target) {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  Assembler::set_target_address_at(pc_ + 1, target);
}


Object* RelocInfo::call_object() {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  return *call_object_address();
}


void RelocInfo::set_call_object(Object* target) {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  *call_object_address() = target;
}


Object** RelocInfo::call_object_address() {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  return reinterpret_cast<Object**>(pc_ + 1);
}

} }  // namespace v8::internal

#endif  // V8_X64_ASSEMBLER_X64_INL_H_
