// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "fast-codegen.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

void FastCodeGenerator::EmitLoadReceiver(Register reg) {
  // Offset 2 is due to return address and saved frame pointer.
  int index = 2 + scope()->num_parameters();
  __ movq(reg, Operand(rbp, index * kPointerSize));
}


void FastCodeGenerator::EmitReceiverMapCheck() {
  Comment cmnt(masm(), ";; MapCheck(this)");
  if (FLAG_print_ir) {
    PrintF("MapCheck(this)\n");
  }

  ASSERT(info()->has_receiver() && info()->receiver()->IsHeapObject());
  Handle<HeapObject> object = Handle<HeapObject>::cast(info()->receiver());
  Handle<Map> map(object->map());

  EmitLoadReceiver(rdx);
  __ CheckMap(rdx, map, bailout(), false);
}


void FastCodeGenerator::EmitGlobalMapCheck() {
  Comment cmnt(masm(), ";; GlobalMapCheck");
  if (FLAG_print_ir) {
    PrintF(";; GlobalMapCheck()");
  }

  ASSERT(info()->has_global_object());
  Handle<Map> map(info()->global_object()->map());

  __ movq(rbx, CodeGenerator::GlobalObject());
  __ CheckMap(rbx, map, bailout(), true);
}


void FastCodeGenerator::EmitGlobalVariableLoad(Handle<Object> cell) {
  ASSERT(cell->IsJSGlobalPropertyCell());
  __ Move(rax, cell);
  __ movq(rax, FieldOperand(rax, JSGlobalPropertyCell::kValueOffset));
  if (FLAG_debug_code) {
    __ Cmp(rax, Factory::the_hole_value());
    __ Check(not_equal, "DontDelete cells can't contain the hole");
  }
}


void FastCodeGenerator::EmitThisPropertyStore(Handle<String> name) {
  LookupResult lookup;
  info()->receiver()->Lookup(*name, &lookup);

  ASSERT(lookup.holder() == *info()->receiver());
  ASSERT(lookup.type() == FIELD);
  Handle<Map> map(Handle<HeapObject>::cast(info()->receiver())->map());
  int index = lookup.GetFieldIndex() - map->inobject_properties();
  int offset = index * kPointerSize;

  // Negative offsets are inobject properties.
  if (offset < 0) {
    offset += map->instance_size();
    __ movq(rcx, rdx);  // Copy receiver for write barrier.
  } else {
    offset += FixedArray::kHeaderSize;
    __ movq(rcx, FieldOperand(rdx, JSObject::kPropertiesOffset));
  }
  // Perform the store.
  __ movq(FieldOperand(rcx, offset), rax);
  // Preserve value from write barrier in case it's needed.
  __ movq(rbx, rax);
  __ RecordWrite(rcx, offset, rbx, rdi);
}


void FastCodeGenerator::Generate(CompilationInfo* compilation_info) {
  ASSERT(info_ == NULL);
  info_ = compilation_info;

  // Save the caller's frame pointer and set up our own.
  Comment prologue_cmnt(masm(), ";; Prologue");
  __ push(rbp);
  __ movq(rbp, rsp);
  __ push(rsi);  // Context.
  __ push(rdi);  // Closure.
  // Note that we keep a live register reference to esi (context) at this
  // point.

  // Receiver (this) is allocated to rdx if there are this properties.
  if (info()->has_this_properties()) EmitReceiverMapCheck();

  // If there is a global variable access check if the global object
  // is the same as at lazy-compilation time.
  if (info()->has_globals()) EmitGlobalMapCheck();

  VisitStatements(info()->function()->body());

  Comment return_cmnt(masm(), ";; Return(<undefined>)");
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);

  Comment epilogue_cmnt(masm(), ";; Epilogue");
  __ movq(rsp, rbp);
  __ pop(rbp);
  __ ret((scope()->num_parameters() + 1) * kPointerSize);

  __ bind(&bailout_);
}


#undef __


} }  // namespace v8::internal
