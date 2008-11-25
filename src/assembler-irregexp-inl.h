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

// A light-weight assembler for the Regexp2000 byte code.


#include "v8.h"
#include "ast.h"
#include "bytecodes-irregexp.h"
#include "assembler-irregexp.h"


namespace v8 { namespace internal {


void IrregexpAssembler::Emit(uint32_t byte) {
  ASSERT(pc_ <= buffer_.length());
  if (pc_ == buffer_.length()) {
    Expand();
  }
  buffer_[pc_++] = byte;
}


void IrregexpAssembler::Emit16(uint32_t word) {
  ASSERT(pc_ <= buffer_.length());
  if (pc_ + 1 >= buffer_.length()) {
    Expand();
  }
  Store16(buffer_.start() + pc_, word);
  pc_ += 2;
}


void IrregexpAssembler::Emit32(uint32_t word) {
  ASSERT(pc_ <= buffer_.length());
  if (pc_ + 3 >= buffer_.length()) {
    Expand();
  }
  Store32(buffer_.start() + pc_, word);
  pc_ += 4;
}


void IrregexpAssembler::EmitOrLink(Label* l) {
    if (l->is_bound()) {
      Emit32(l->pos());
    } else {
      int pos = 0;
      if (l->is_linked()) {
        pos = l->pos();
      }
      l->link_to(pc_);
      Emit32(pos);
    }
  }

} }  // namespace v8::internal
