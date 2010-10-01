// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
//

#ifndef V8_HANDLES_INL_H_
#define V8_HANDLES_INL_H_

#include "api.h"
#include "apiutils.h"
#include "handles.h"
#include "isolate.h"

namespace v8 {
namespace internal {

template<class T>
Handle<T>::Handle(T* obj) {
  ASSERT(!obj->IsFailure());
  location_ = HandleScope::CreateHandle(obj, Isolate::Current());
}

template<class T>
Handle<T>::Handle(HeapObject* obj) {
  location_ = HandleScope::CreateHandle<T>(obj, obj->GetIsolate());
}


template<class T>
Handle<T>::Handle(T* obj, Isolate* isolate) {
  ASSERT(!obj->IsFailure());
  location_ = HandleScope::CreateHandle(obj, isolate);
}


template <class T>
inline T* Handle<T>::operator*() const {
  ASSERT(location_ != NULL);
  ASSERT(reinterpret_cast<Address>(*location_) != kHandleZapValue);
  return *BitCast<T**>(location_);
}


// Helper class to zero out the number of extensions in the handle
// scope data after it has been saved.
// This is only necessary for HandleScope constructor to get the right
// order of effects.
class HandleScopeDataTransfer {
 public:
  typedef v8::ImplementationUtilities::HandleScopeData Data;

  explicit HandleScopeDataTransfer(Data* data) : data_(data) {}
  ~HandleScopeDataTransfer() { data_->extensions = 0; }

  // Called before the destructor to get the data to save.
  Data* data() { return data_; }

 private:
  Data* data_;

  DISALLOW_COPY_AND_ASSIGN(HandleScopeDataTransfer);
};


HandleScope::HandleScope()
    : previous_(*HandleScopeDataTransfer(
        Isolate::Current()->handle_scope_data()).data()) {
}


HandleScope::HandleScope(Isolate* isolate)
    : previous_(*HandleScopeDataTransfer(isolate->handle_scope_data()).data()) {
  ASSERT(isolate == Isolate::Current());
}


template <typename T>
T** HandleScope::CreateHandle(T* value, Isolate* isolate) {
  ASSERT(isolate == Isolate::Current());
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate->handle_scope_data();

  internal::Object** cur = current->next;
  if (cur == current->limit) cur = Extend();
  // Update the current next field, set the value in the created
  // handle, and return the result.
  ASSERT(cur < current->limit);
  current->next = cur + 1;

  T** result = reinterpret_cast<T**>(cur);
  *result = value;
  return result;
}


void HandleScope::Enter(
    v8::ImplementationUtilities::HandleScopeData* previous) {
  v8::ImplementationUtilities::HandleScopeData* current =
      Isolate::Current()->handle_scope_data();
  *previous = *current;
  current->extensions = 0;
}


void HandleScope::Leave(
    const v8::ImplementationUtilities::HandleScopeData* previous) {
  Isolate* isolate = previous->isolate;
  ASSERT(isolate == Isolate::Current());
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate->handle_scope_data();
  if (current->extensions > 0) {
    DeleteExtensions(isolate);
  }
  *current = *previous;
#ifdef DEBUG
  ZapRange(current->next, current->limit);
#endif
}


#ifdef DEBUG
inline NoHandleAllocation::NoHandleAllocation() {
  v8::ImplementationUtilities::HandleScopeData* current =
      Isolate::Current()->handle_scope_data();
  extensions_ = current->extensions;
  // Shrink the current handle scope to make it impossible to do
  // handle allocations without an explicit handle scope.
  current->limit = current->next;
  current->extensions = -1;
}


inline NoHandleAllocation::~NoHandleAllocation() {
  // Restore state in current handle scope to re-enable handle
  // allocations.
  Isolate::Current()->handle_scope_data()->extensions = extensions_;
}
#endif


} }  // namespace v8::internal

#endif  // V8_HANDLES_INL_H_
