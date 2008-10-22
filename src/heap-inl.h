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

#ifndef V8_HEAP_INL_H_
#define V8_HEAP_INL_H_

#include "log.h"
#include "v8-counters.h"

namespace v8 { namespace internal {

int Heap::MaxHeapObjectSize() {
  return Page::kMaxHeapObjectSize;
}


Object* Heap::AllocateRaw(int size_in_bytes,
                          AllocationSpace space) {
  ASSERT(allocation_allowed_ && gc_state_ == NOT_IN_GC);
#ifdef DEBUG
  if (FLAG_gc_interval >= 0 &&
      !disallow_allocation_failure_ &&
      Heap::allocation_timeout_-- <= 0) {
    return Failure::RetryAfterGC(size_in_bytes, space);
  }
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  if (NEW_SPACE == space) {
    return new_space_.AllocateRaw(size_in_bytes);
  }

  Object* result;
  if (OLD_POINTER_SPACE == space) {
    result = old_pointer_space_->AllocateRaw(size_in_bytes);
  } else if (OLD_DATA_SPACE == space) {
    result = old_data_space_->AllocateRaw(size_in_bytes);
  } else if (CODE_SPACE == space) {
    result = code_space_->AllocateRaw(size_in_bytes);
  } else if (LO_SPACE == space) {
    result = lo_space_->AllocateRaw(size_in_bytes);
  } else {
    ASSERT(MAP_SPACE == space);
    result = map_space_->AllocateRaw(size_in_bytes);
  }
  if (result->IsFailure()) old_gen_exhausted_ = true;
  return result;
}


Object* Heap::NumberFromInt32(int32_t value) {
  if (Smi::IsValid(value)) return Smi::FromInt(value);
  // Bypass NumberFromDouble to avoid various redundant checks.
  return AllocateHeapNumber(FastI2D(value));
}


Object* Heap::NumberFromUint32(uint32_t value) {
  if ((int32_t)value >= 0 && Smi::IsValid((int32_t)value)) {
    return Smi::FromInt((int32_t)value);
  }
  // Bypass NumberFromDouble to avoid various redundant checks.
  return AllocateHeapNumber(FastUI2D(value));
}


Object* Heap::AllocateRawMap(int size_in_bytes) {
#ifdef DEBUG
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  Object* result = map_space_->AllocateRaw(size_in_bytes);
  if (result->IsFailure()) old_gen_exhausted_ = true;
  return result;
}


bool Heap::InNewSpace(Object* object) {
  return new_space_.Contains(object);
}


bool Heap::InFromSpace(Object* object) {
  return new_space_.FromSpaceContains(object);
}


bool Heap::InToSpace(Object* object) {
  return new_space_.ToSpaceContains(object);
}


bool Heap::ShouldBePromoted(Address old_address, int object_size) {
  // An object should be promoted if:
  // - the object has survived a scavenge operation or
  // - to space is already 25% full.
  return old_address < new_space_.age_mark()
      || (new_space_.Size() + object_size) >= (new_space_.Capacity() >> 2);
}


void Heap::RecordWrite(Address address, int offset) {
  if (new_space_.Contains(address)) return;
  ASSERT(!new_space_.FromSpaceContains(address));
  SLOW_ASSERT(Contains(address + offset));
  Page::SetRSet(address, offset);
}


OldSpace* Heap::TargetSpace(HeapObject* object) {
  // Heap numbers and sequential strings are promoted to old data space, all
  // other object types are promoted to old pointer space.  We do not use
  // object->IsHeapNumber() and object->IsSeqString() because we already
  // know that object has the heap object tag.
  InstanceType type = object->map()->instance_type();
  ASSERT((type != CODE_TYPE) && (type != MAP_TYPE));
  bool has_pointers =
      type != HEAP_NUMBER_TYPE &&
      (type >= FIRST_NONSTRING_TYPE ||
       String::cast(object)->representation_tag() != kSeqStringTag);
  return has_pointers ? old_pointer_space_ : old_data_space_;
}


void Heap::CopyBlock(Object** dst, Object** src, int byte_size) {
  ASSERT(IsAligned(byte_size, kPointerSize));

  // Use block copying memcpy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  static const int kBlockCopyLimit = 16 * kPointerSize;

  if (byte_size >= kBlockCopyLimit) {
    memcpy(dst, src, byte_size);
  } else {
    int remaining = byte_size / kPointerSize;
    do {
      remaining--;
      *dst++ = *src++;
    } while (remaining > 0);
  }
}


#define GC_GREEDY_CHECK() \
  ASSERT(!FLAG_gc_greedy || v8::internal::Heap::GarbageCollectionGreedyCheck())

// Do not use the identifier __object__ in a call to this macro.
//
// Call the function FUNCTION_CALL.  If it fails with a RetryAfterGC
// failure, call the garbage collector and retry the function.  If the
// garbage collector cannot reclaim the required space or the second
// call fails with a RetryAfterGC failure, fail with out of memory.
// If there is any other failure, return a null handle.  If either
// call succeeds, return a handle to the functions return value.
//
// Note that this macro always returns or raises a fatal error.
#define CALL_HEAP_FUNCTION(FUNCTION_CALL, TYPE)                              \
  do {                                                                       \
    GC_GREEDY_CHECK();                                                       \
    Object* __object__ = FUNCTION_CALL;                                      \
    if (__object__->IsFailure()) {                                           \
      if (__object__->IsRetryAfterGC()) {                                    \
        if (!Heap::CollectGarbage(                                           \
                Failure::cast(__object__)->requested(),                      \
                Failure::cast(__object__)->allocation_space())) {            \
          /* TODO(1181417): Fix this. */                                     \
          v8::internal::V8::FatalProcessOutOfMemory("CALL_HEAP_FUNCTION");   \
        }                                                                    \
        __object__ = FUNCTION_CALL;                                          \
        if (__object__->IsFailure()) {                                       \
          if (__object__->IsRetryAfterGC()) {                                \
            /* TODO(1181417): Fix this. */                                   \
            v8::internal::V8::FatalProcessOutOfMemory("CALL_HEAP_FUNCTION"); \
          }                                                                  \
          return Handle<TYPE>();                                             \
        }                                                                    \
      } else {                                                               \
        if (__object__->IsOutOfMemoryFailure()) {                            \
          v8::internal::V8::FatalProcessOutOfMemory("CALL_HEAP_FUNCTION");   \
        }                                                                    \
        return Handle<TYPE>();                                               \
      }                                                                      \
    }                                                                        \
    return Handle<TYPE>(TYPE::cast(__object__));                             \
  } while (false)


// Don't use the following names: __object__, __failure__.
#define CALL_HEAP_FUNCTION_VOID(FUNCTION_CALL)                      \
  GC_GREEDY_CHECK();                                                \
  Object* __object__ = FUNCTION_CALL;                               \
  if (__object__->IsFailure()) {                                    \
    if (__object__->IsRetryAfterGC()) {                             \
      Failure* __failure__ = Failure::cast(__object__);             \
      if (!Heap::CollectGarbage(__failure__->requested(),           \
                                __failure__->allocation_space())) { \
         /* TODO(1181417): Fix this. */                             \
         V8::FatalProcessOutOfMemory("Handles");                    \
      }                                                             \
      __object__ = FUNCTION_CALL;                                   \
      if (__object__->IsFailure()) {                                \
        if (__object__->IsRetryAfterGC()) {                         \
           /* TODO(1181417): Fix this. */                           \
           V8::FatalProcessOutOfMemory("Handles");                  \
        }                                                           \
        return;                                                     \
      }                                                             \
    } else {                                                        \
      if (__object__->IsOutOfMemoryFailure()) {                     \
         V8::FatalProcessOutOfMemory("Handles");                    \
      }                                                             \
      UNREACHABLE();                                                \
    }                                                               \
  }


#ifdef DEBUG

inline bool Heap::allow_allocation(bool new_state) {
  bool old = allocation_allowed_;
  allocation_allowed_ = new_state;
  return old;
}

#endif


} }  // namespace v8::internal

#endif  // V8_HEAP_INL_H_
