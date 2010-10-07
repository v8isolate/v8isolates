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

#ifndef V8_ALLOCATION_INL_H_
#define V8_ALLOCATION_INL_H_

#include "allocation.h"

namespace v8 {
namespace internal {


NativeAllocationChecker::NativeAllocationChecker(
    NativeAllocationChecker::NativeAllocationAllowed allowed)
    : allowed_(allowed) {
#ifdef DEBUG
  if (allowed == DISALLOW) {
    Isolate* isolate = Isolate::Current();
    isolate->set_allocation_disallowed(isolate->allocation_disallowed() + 1);
  }
#endif
}


NativeAllocationChecker::~NativeAllocationChecker() {
#ifdef DEBUG
  Isolate* isolate = Isolate::Current();
  if (allowed_ == DISALLOW) {
    isolate->set_allocation_disallowed(isolate->allocation_disallowed() - 1);
  }
  ASSERT(isolate->allocation_disallowed() >= 0);
#endif
}


bool NativeAllocationChecker::allocation_allowed() {
#ifdef DEBUG
  return Isolate::Current()->allocation_disallowed() == 0;
#else
  return true;
#endif  // DEBUG
}


void* PreallocatedStorage::New(size_t size) {
  return Isolate::Current()->PreallocatedStorageNew(size);
}


void PreallocatedStorage::Delete(void* p) {
  return Isolate::Current()->PreallocatedStorageDelete(p);
}


} }  // namespace v8::internal

#endif  // V8_ALLOCATION_INL_H_
