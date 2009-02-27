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

// Test comparison operations that involve one or two constant smis.

function test() {
  var i = 5;
  var j = 3;

  assertTrue( j < i );
  i = 5; j = 3;
  assertTrue( j <= i );
  i = 5; j = 3;
  assertTrue( i > j );
  i = 5; j = 3;
  assertTrue( i >= j );
  i = 5; j = 3;
  assertTrue( i != j );
  i = 5; j = 3;
  assertTrue( i == i );
  i = 5; j = 3;
  assertFalse( i < j );
  i = 5; j = 3;
  assertFalse( i <= j );
  i = 5; j = 3;
  assertFalse( j > i );
  i = 5; j = 3;
  assertFalse(j >= i );
  i = 5; j = 3;
  assertFalse( j == i);
  i = 5; j = 3;
  assertFalse( i != i);

  i = 10 * 10;
  while ( i < 107 ) {
    ++i;
  }
  j = 21;

  assertTrue( j < i );
  j = 21;
  assertTrue( j <= i );
  j = 21;
  assertTrue( i > j );
  j = 21;
  assertTrue( i >= j );
  j = 21;
  assertTrue( i != j );
  j = 21;
  assertTrue( i == i );
  j = 21;
  assertFalse( i < j );
  j = 21;
  assertFalse( i <= j );
  j = 21;
  assertFalse( j > i );
  j = 21;
  assertFalse(j >= i );
  j = 21;
  assertFalse( j == i);
  j = 21;
  assertFalse( i != i);
  j = 21;
  assertTrue( j == j );
  j = 21;
  assertFalse( j != j );

  assertTrue( 100 > 99 );
  assertTrue( 101 >= 90 );
  assertTrue( 11111 > -234 );
  assertTrue( -888 <= -20 );

  while ( 234 > 456 ) {
    i = i + 1;
  }

  switch(3) {
    case 5:
      assertUnreachable();
      break;
    case 3:
      j = 13;
    default:
      i = 2;
    case 7:
      j = 17;
      break;
    case 9:
      j = 19;
      assertUnreachable();
      break;
  }
  assertEquals(17, j, "switch with constant value");
}

test();

