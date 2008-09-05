// Copyright 2006-2008 Google Inc. All Rights Reserved.
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

// To avoid warnings from <map> on windows we disable exceptions.
#define _HAS_EXCEPTIONS 0
#include <signal.h>
#include <string>
#include <map>

#include "v8.h"

#include "bootstrapper.h"
#include "natives.h"
#include "platform.h"
#include "serialize.h"

DEFINE_bool(h, false, "print this message");

namespace v8 { namespace internal {
#ifdef ENABLE_LOGGING_AND_PROFILING
  DECLARE_bool(log_code);
#endif
} }

// use explicit namespace to avoid clashing with types in namespace v8
namespace i = v8::internal;
using namespace v8;

static const unsigned int kMaxCounters = 256;

// A single counter in a counter collection.
class Counter {
 public:
  static const int kMaxNameSize = 64;
  int32_t* Bind(const wchar_t* name) {
    int i;
    for (i = 0; i < kMaxNameSize - 1 && name[i]; i++) {
      name_[i] = static_cast<char>(name[i]);
    }
    name_[i] = '\0';
    return &counter_;
  }
 private:
  int32_t counter_;
  uint8_t name_[kMaxNameSize];
};


// A set of counters and associated information.  An instance of this
// class is stored directly in the memory-mapped counters file if
// the --save-counters options is used
class CounterCollection {
 public:
  CounterCollection() {
    magic_number_ = 0xDEADFACE;
    max_counters_ = kMaxCounters;
    max_name_size_ = Counter::kMaxNameSize;
    counters_in_use_ = 0;
  }
  Counter* GetNextCounter() {
    if (counters_in_use_ == kMaxCounters) return NULL;
    return &counters_[counters_in_use_++];
  }
 private:
  uint32_t magic_number_;
  uint32_t max_counters_;
  uint32_t max_name_size_;
  uint32_t counters_in_use_;
  Counter counters_[kMaxCounters];
};


// We statically allocate a set of local counters to be used if we
// don't want to store the stats in a memory-mapped file
static CounterCollection local_counters;
static CounterCollection* counters = &local_counters;


typedef std::map<std::wstring, int*> CounterMap;
typedef std::map<std::wstring, int*>::iterator CounterMapIterator;
static CounterMap counter_table_;

// Callback receiver when v8 has a counter to track.
static int* counter_callback(const wchar_t* name) {
  std::wstring counter = name;
  // See if this counter name is already known.
  if (counter_table_.find(counter) != counter_table_.end())
    return counter_table_[counter];

  Counter* ctr = counters->GetNextCounter();
  if (ctr == NULL) return NULL;
  int* ptr = ctr->Bind(name);
  counter_table_[counter] = ptr;
  return ptr;
}


// Write C++ code that defines Snapshot::snapshot_ to contain the snapshot
// to the file given by filename. Only the first size chars are written.
static int WriteInternalSnapshotToFile(const char* filename,
                                       const char* str,
                                       int size) {
  FILE* f = fopen(filename, "wb");
  if (f == NULL) {
    i::OS::PrintError("Cannot open file %s for reading.\n", filename);
    return 0;
  }
  fprintf(f, "// Autogenerated snapshot file. Do not edit.\n\n");
  fprintf(f, "#include \"v8.h\"\n");
  fprintf(f, "#include \"platform.h\"\n\n");
  fprintf(f, "#include \"snapshot.h\"\n\n");
  fprintf(f, "namespace v8 {\nnamespace internal {\n\n");
  fprintf(f, "const char Snapshot::data_[] = {");
  int written = 0;
  written += fprintf(f, "%i", str[0]);
  for (int i = 1; i < size; ++i) {
    written += fprintf(f, ",%i", str[i]);
    // The following is needed to keep the line length low on Visual C++:
    if (i % 512 == 0) fprintf(f, "\n");
  }
  fprintf(f, "};\n\n");
  fprintf(f, "int Snapshot::size_ = %d;\n\n", size);
  fprintf(f, "} }  // namespace v8::internal\n");
  fclose(f);
  return written;
}


int main(int argc, char** argv) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // By default, log code create information in the snapshot.
  i::FLAG_log_code = true;
#endif
  // Print the usage if an error occurs when parsing the command line
  // flags or if the help flag is set.
  int result = i::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (result > 0 || argc != 2 || FLAG_h) {
    ::printf("Usage: %s [flag] ... outfile\n", argv[0]);
    i::FlagList::Print(NULL, false);
    return !FLAG_h;
  }

  v8::V8::SetCounterFunction(counter_callback);
  v8::HandleScope scope;

  const int kExtensionCount = 5;
  const char* extension_list[kExtensionCount] = { "v8/print",
                                                  "v8/load",
                                                  "v8/quit",
                                                  "v8/version",
                                                  "v8/gc" };
  v8::ExtensionConfiguration extensions(kExtensionCount, extension_list);
  v8::Context::New(&extensions);

  // Make sure all builtin scripts are cached.
  { HandleScope scope;
    for (int i = 0; i < i::Natives::GetBuiltinsCount(); i++) {
      i::Bootstrapper::NativesSourceLookup(i);
    }
  }
  // Get rid of unreferenced scripts with a global GC.
  i::Heap::CollectAllGarbage();
  i::Serializer ser;
  ser.Serialize();
  char* str;
  int len;
  ser.Finalize(&str, &len);

  WriteInternalSnapshotToFile(argv[1], str, len);

  i::DeleteArray(str);

  return 0;
}
