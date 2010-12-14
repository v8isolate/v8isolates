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

#include <stdlib.h>

#include "v8.h"

#include "api.h"
#include "bootstrapper.h"
#include "codegen-inl.h"
#include "debug.h"
#include "simulator.h"
#include "v8threads.h"

namespace v8 {
namespace internal {


StackGuard::StackGuard()
    : isolate_(NULL) {
}


void StackGuard::set_interrupt_limits(const ExecutionAccess& lock) {
  ASSERT(isolate_ != NULL);
  // Ignore attempts to interrupt when interrupts are postponed.
  if (should_postpone_interrupts(lock)) return;
  thread_local_.jslimit_ = kInterruptLimit;
  thread_local_.climit_ = kInterruptLimit;
  isolate_->heap()->SetStackLimits();
}


void StackGuard::reset_limits(const ExecutionAccess& lock) {
  ASSERT(isolate_ != NULL);
  thread_local_.jslimit_ = thread_local_.real_jslimit_;
  thread_local_.climit_ = thread_local_.real_climit_;
  isolate_->heap()->SetStackLimits();
}


static Handle<Object> Invoke(bool construct,
                             Handle<JSFunction> func,
                             Handle<Object> receiver,
                             int argc,
                             Object*** args,
                             bool* has_pending_exception) {
  Isolate* isolate = func->GetIsolate();

  // Entering JavaScript.
  VMState state(isolate, JS);

  // Placeholder for return value.
  MaybeObject* value = reinterpret_cast<Object*>(kZapValue);

  typedef Object* (*JSEntryFunction)(
    byte* entry,
    Object* function,
    Object* receiver,
    int argc,
    Object*** args);

  Handle<Code> code;
  if (construct) {
    JSConstructEntryStub stub;
    code = stub.GetCode();
  } else {
    JSEntryStub stub;
    code = stub.GetCode();
  }

  // Convert calls on global objects to be calls on the global
  // receiver instead to avoid having a 'this' pointer which refers
  // directly to a global object.
  if (receiver->IsGlobalObject()) {
    Handle<GlobalObject> global = Handle<GlobalObject>::cast(receiver);
    receiver = Handle<JSObject>(global->global_receiver());
  }

  // Make sure that the global object of the context we're about to
  // make the current one is indeed a global object.
  ASSERT(func->context()->global()->IsGlobalObject());

  {
    // Save and restore context around invocation and block the
    // allocation of handles without explicit handle scopes.
    SaveContext save(isolate);
    NoHandleAllocation na;
    JSEntryFunction entry = FUNCTION_CAST<JSEntryFunction>(code->entry());

    // Call the function through the right JS entry stub.
    byte* entry_address = func->code()->entry();
    JSFunction* function = *func;
    Object* receiver_pointer = *receiver;
    value = CALL_GENERATED_CODE(entry, entry_address, function,
                                receiver_pointer, argc, args);
  }

#ifdef DEBUG
  value->Verify();
#endif

  // Update the pending exception flag and return the value.
  *has_pending_exception = value->IsException();
  ASSERT(*has_pending_exception == Isolate::Current()->has_pending_exception());
  if (*has_pending_exception) {
    isolate->ReportPendingMessages();
    return Handle<Object>();
  } else {
    isolate->clear_pending_message();
  }

  return Handle<Object>(value->ToObjectUnchecked(), isolate);
}


Handle<Object> Execution::Call(Handle<JSFunction> func,
                               Handle<Object> receiver,
                               int argc,
                               Object*** args,
                               bool* pending_exception) {
  return Invoke(false, func, receiver, argc, args, pending_exception);
}


Handle<Object> Execution::New(Handle<JSFunction> func, int argc,
                              Object*** args, bool* pending_exception) {
  return Invoke(true, func, Isolate::Current()->global(), argc, args,
                pending_exception);
}


Handle<Object> Execution::TryCall(Handle<JSFunction> func,
                                  Handle<Object> receiver,
                                  int argc,
                                  Object*** args,
                                  bool* caught_exception) {
  // Enter a try-block while executing the JavaScript code. To avoid
  // duplicate error printing it must be non-verbose.  Also, to avoid
  // creating message objects during stack overflow we shouldn't
  // capture messages.
  v8::TryCatch catcher;
  catcher.SetVerbose(false);
  catcher.SetCaptureMessage(false);

  Handle<Object> result = Invoke(false, func, receiver, argc, args,
                                 caught_exception);

  if (*caught_exception) {
    ASSERT(catcher.HasCaught());
    Isolate* isolate = Isolate::Current();
    ASSERT(isolate->has_pending_exception());
    ASSERT(isolate->external_caught_exception());
    if (isolate->pending_exception() ==
        isolate->heap()->termination_exception()) {
      result = isolate->factory()->termination_exception();
    } else {
      result = v8::Utils::OpenHandle(*catcher.Exception());
    }
    isolate->OptionalRescheduleException(true);
  }

  ASSERT(!Isolate::Current()->has_pending_exception());
  ASSERT(!Isolate::Current()->external_caught_exception());
  return result;
}


Handle<Object> Execution::GetFunctionDelegate(Handle<Object> object) {
  ASSERT(!object->IsJSFunction());

  // If you return a function from here, it will be called when an
  // attempt is made to call the given object as a function.

  // Regular expressions can be called as functions in both Firefox
  // and Safari so we allow it too.
  if (object->IsJSRegExp()) {
    Handle<String> exec = FACTORY->exec_symbol();
    // TODO(lrn): Bug 617.  We should use the default function here, not the
    // one on the RegExp object.
    Object* exec_function;
    { MaybeObject* maybe_exec_function = object->GetProperty(*exec);
      // This can lose an exception, but the alternative is to put a failure
      // object in a handle, which is not GC safe.
      if (!maybe_exec_function->ToObject(&exec_function)) {
        return FACTORY->undefined_value();
      }
    }
    return Handle<Object>(exec_function);
  }

  // Objects created through the API can have an instance-call handler
  // that should be used when calling the object as a function.
  if (object->IsHeapObject() &&
      HeapObject::cast(*object)->map()->has_instance_call_handler()) {
    return Handle<JSFunction>(
        Isolate::Current()->global_context()->call_as_function_delegate());
  }

  return FACTORY->undefined_value();
}


Handle<Object> Execution::GetConstructorDelegate(Handle<Object> object) {
  ASSERT(!object->IsJSFunction());

  // If you return a function from here, it will be called when an
  // attempt is made to call the given object as a constructor.

  // Objects created through the API can have an instance-call handler
  // that should be used when calling the object as a function.
  if (object->IsHeapObject() &&
      HeapObject::cast(*object)->map()->has_instance_call_handler()) {
    return Handle<JSFunction>(
        Isolate::Current()->global_context()->call_as_constructor_delegate());
  }

  return FACTORY->undefined_value();
}


bool StackGuard::IsStackOverflow() {
  ExecutionAccess access;
  return (thread_local_.jslimit_ != kInterruptLimit &&
          thread_local_.climit_ != kInterruptLimit);
}


void StackGuard::EnableInterrupts() {
  ExecutionAccess access;
  if (has_pending_interrupts(access)) {
    set_interrupt_limits(access);
  }
}


void StackGuard::SetStackLimit(uintptr_t limit) {
  ExecutionAccess access;
  // If the current limits are special (eg due to a pending interrupt) then
  // leave them alone.
  uintptr_t jslimit = SimulatorStack::JsLimitFromCLimit(limit);
  if (thread_local_.jslimit_ == thread_local_.real_jslimit_) {
    thread_local_.jslimit_ = jslimit;
  }
  if (thread_local_.climit_ == thread_local_.real_climit_) {
    thread_local_.climit_ = limit;
  }
  thread_local_.real_climit_ = limit;
  thread_local_.real_jslimit_ = jslimit;
}


void StackGuard::DisableInterrupts() {
  ExecutionAccess access;
  reset_limits(access);
}


bool StackGuard::IsInterrupted() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & INTERRUPT;
}


void StackGuard::Interrupt() {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ |= INTERRUPT;
  set_interrupt_limits(access);
}


bool StackGuard::IsPreempted() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & PREEMPT;
}


void StackGuard::Preempt() {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ |= PREEMPT;
  set_interrupt_limits(access);
}


bool StackGuard::IsTerminateExecution() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & TERMINATE;
}


void StackGuard::TerminateExecution() {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ |= TERMINATE;
  set_interrupt_limits(access);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
bool StackGuard::IsDebugBreak() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & DEBUGBREAK;
}


void StackGuard::DebugBreak() {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ |= DEBUGBREAK;
  set_interrupt_limits(access);
}


bool StackGuard::IsDebugCommand() {
  ExecutionAccess access;
  return thread_local_.interrupt_flags_ & DEBUGCOMMAND;
}


void StackGuard::DebugCommand() {
  if (FLAG_debugger_auto_break) {
    ExecutionAccess access;
    thread_local_.interrupt_flags_ |= DEBUGCOMMAND;
    set_interrupt_limits(access);
  }
}
#endif

void StackGuard::Continue(InterruptFlag after_what) {
  ExecutionAccess access;
  thread_local_.interrupt_flags_ &= ~static_cast<int>(after_what);
  if (!should_postpone_interrupts(access) && !has_pending_interrupts(access)) {
    reset_limits(access);
  }
}


char* StackGuard::ArchiveStackGuard(char* to) {
  ExecutionAccess access;
  memcpy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
  ThreadLocal blank;

  // Set the stack limits using the old thread_local_.
  // TODO(isolates): This was the old semantics of constructing a ThreadLocal
  //                 (as the ctor called SetStackLimits, which looked at the
  //                 current thread_local_ from StackGuard)-- but is this
  //                 really what was intended?
  isolate_->heap()->SetStackLimits();
  thread_local_ = blank;

  return to + sizeof(ThreadLocal);
}


char* StackGuard::RestoreStackGuard(char* from) {
  ExecutionAccess access;
  memcpy(reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
  isolate_->heap()->SetStackLimits();
  return from + sizeof(ThreadLocal);
}


void StackGuard::FreeThreadResources() {
  Isolate::CurrentPerIsolateThreadData()->set_stack_limit(
      thread_local_.real_climit_);
}


void StackGuard::ThreadLocal::Clear() {
  real_jslimit_ = kIllegalLimit;
  jslimit_ = kIllegalLimit;
  real_climit_ = kIllegalLimit;
  climit_ = kIllegalLimit;
  nesting_ = 0;
  postpone_interrupts_nesting_ = 0;
  interrupt_flags_ = 0;
}


bool StackGuard::ThreadLocal::Initialize() {
  bool should_set_stack_limits = false;
  if (real_climit_ == kIllegalLimit) {
    // Takes the address of the limit variable in order to find out where
    // the top of stack is right now.
    uintptr_t limit = reinterpret_cast<uintptr_t>(&limit) - kLimitSize;
    ASSERT(reinterpret_cast<uintptr_t>(&limit) > kLimitSize);
    real_jslimit_ = SimulatorStack::JsLimitFromCLimit(limit);
    jslimit_ = SimulatorStack::JsLimitFromCLimit(limit);
    real_climit_ = limit;
    climit_ = limit;
    should_set_stack_limits = true;
  }
  nesting_ = 0;
  postpone_interrupts_nesting_ = 0;
  interrupt_flags_ = 0;
  return should_set_stack_limits;
}


void StackGuard::ClearThread(const ExecutionAccess& lock) {
  thread_local_.Clear();
  isolate_->heap()->SetStackLimits();
}


void StackGuard::InitThread(const ExecutionAccess& lock) {
  if (thread_local_.Initialize()) isolate_->heap()->SetStackLimits();
  uintptr_t stored_limit =
      Isolate::CurrentPerIsolateThreadData()->stack_limit();
  // You should hold the ExecutionAccess lock when you call this.
  if (stored_limit != 0) {
    StackGuard::SetStackLimit(stored_limit);
  }
}


// --- C a l l s   t o   n a t i v e s ---

#define RETURN_NATIVE_CALL(name, argc, argv, has_pending_exception)            \
  do {                                                                         \
    Object** args[argc] = argv;                                                \
    ASSERT(has_pending_exception != NULL);                                     \
    return Call(Isolate::Current()->name##_fun(),                              \
                Isolate::Current()->js_builtins_object(), argc, args,          \
                has_pending_exception);                                        \
  } while (false)


Handle<Object> Execution::ToBoolean(Handle<Object> obj) {
  // See the similar code in runtime.js:ToBoolean.
  if (obj->IsBoolean()) return obj;
  bool result = true;
  if (obj->IsString()) {
    result = Handle<String>::cast(obj)->length() != 0;
  } else if (obj->IsNull() || obj->IsUndefined()) {
    result = false;
  } else if (obj->IsNumber()) {
    double value = obj->Number();
    result = !((value == 0) || isnan(value));
  }
  return Handle<Object>(HEAP->ToBoolean(result));
}


Handle<Object> Execution::ToNumber(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_number, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToString(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_string, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToDetailString(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_detail_string, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToObject(Handle<Object> obj, bool* exc) {
  if (obj->IsJSObject()) return obj;
  RETURN_NATIVE_CALL(to_object, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToInteger(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_integer, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToUint32(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_uint32, 1, { obj.location() }, exc);
}


Handle<Object> Execution::ToInt32(Handle<Object> obj, bool* exc) {
  RETURN_NATIVE_CALL(to_int32, 1, { obj.location() }, exc);
}


Handle<Object> Execution::NewDate(double time, bool* exc) {
  Handle<Object> time_obj = FACTORY->NewNumber(time);
  RETURN_NATIVE_CALL(create_date, 1, { time_obj.location() }, exc);
}


#undef RETURN_NATIVE_CALL


Handle<JSRegExp> Execution::NewJSRegExp(Handle<String> pattern,
                                        Handle<String> flags,
                                        bool* exc) {
  Handle<JSFunction> function = Handle<JSFunction>(
      pattern->GetIsolate()->global_context()->regexp_function());
  Handle<Object> re_obj = RegExpImpl::CreateRegExpLiteral(
      function, pattern, flags, exc);
  if (*exc) return Handle<JSRegExp>();
  return Handle<JSRegExp>::cast(re_obj);
}


Handle<Object> Execution::CharAt(Handle<String> string, uint32_t index) {
  int int_index = static_cast<int>(index);
  if (int_index < 0 || int_index >= string->length()) {
    return FACTORY->undefined_value();
  }

  Handle<Object> char_at =
      GetProperty(Isolate::Current()->js_builtins_object(),
                  FACTORY->char_at_symbol());
  if (!char_at->IsJSFunction()) {
    return FACTORY->undefined_value();
  }

  bool caught_exception;
  Handle<Object> index_object = FACTORY->NewNumberFromInt(int_index);
  Object** index_arg[] = { index_object.location() };
  Handle<Object> result = TryCall(Handle<JSFunction>::cast(char_at),
                                  string,
                                  ARRAY_SIZE(index_arg),
                                  index_arg,
                                  &caught_exception);
  if (caught_exception) {
    return FACTORY->undefined_value();
  }
  return result;
}


Handle<JSFunction> Execution::InstantiateFunction(
    Handle<FunctionTemplateInfo> data, bool* exc) {
  // Fast case: see if the function has already been instantiated
  int serial_number = Smi::cast(data->serial_number())->value();
  Object* elm =
      Isolate::Current()->global_context()->function_cache()->
          GetElementNoExceptionThrown(serial_number);
  if (elm->IsJSFunction()) return Handle<JSFunction>(JSFunction::cast(elm));
  // The function has not yet been instantiated in this context; do it.
  Object** args[1] = { Handle<Object>::cast(data).location() };
  Handle<Object> result =
      Call(Isolate::Current()->instantiate_fun(),
           Isolate::Current()->js_builtins_object(), 1, args, exc);
  if (*exc) return Handle<JSFunction>::null();
  return Handle<JSFunction>::cast(result);
}


Handle<JSObject> Execution::InstantiateObject(Handle<ObjectTemplateInfo> data,
                                              bool* exc) {
  if (data->property_list()->IsUndefined() &&
      !data->constructor()->IsUndefined()) {
    // Initialization to make gcc happy.
    Object* result = NULL;
    {
      HandleScope scope;
      Handle<FunctionTemplateInfo> cons_template =
          Handle<FunctionTemplateInfo>(
              FunctionTemplateInfo::cast(data->constructor()));
      Handle<JSFunction> cons = InstantiateFunction(cons_template, exc);
      if (*exc) return Handle<JSObject>::null();
      Handle<Object> value = New(cons, 0, NULL, exc);
      if (*exc) return Handle<JSObject>::null();
      result = *value;
    }
    ASSERT(!*exc);
    return Handle<JSObject>(JSObject::cast(result));
  } else {
    Object** args[1] = { Handle<Object>::cast(data).location() };
    Handle<Object> result =
        Call(Isolate::Current()->instantiate_fun(),
             Isolate::Current()->js_builtins_object(), 1, args, exc);
    if (*exc) return Handle<JSObject>::null();
    return Handle<JSObject>::cast(result);
  }
}


void Execution::ConfigureInstance(Handle<Object> instance,
                                  Handle<Object> instance_template,
                                  bool* exc) {
  Object** args[2] = { instance.location(), instance_template.location() };
  Execution::Call(Isolate::Current()->configure_instance_fun(),
                  Isolate::Current()->js_builtins_object(), 2, args, exc);
}


Handle<String> Execution::GetStackTraceLine(Handle<Object> recv,
                                            Handle<JSFunction> fun,
                                            Handle<Object> pos,
                                            Handle<Object> is_global) {
  const int argc = 4;
  Object** args[argc] = { recv.location(),
                          Handle<Object>::cast(fun).location(),
                          pos.location(),
                          is_global.location() };
  bool caught_exception = false;
  Handle<Object> result =
      TryCall(Isolate::Current()->get_stack_trace_line_fun(),
              Isolate::Current()->js_builtins_object(), argc, args,
              &caught_exception);
  if (caught_exception || !result->IsString()) return FACTORY->empty_symbol();
  return Handle<String>::cast(result);
}


static Object* RuntimePreempt() {
  Isolate* isolate = Isolate::Current();

  // Clear the preempt request flag.
  isolate->stack_guard()->Continue(PREEMPT);

  ContextSwitcher::PreemptionReceived();

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (isolate->debug()->InDebugger()) {
    // If currently in the debugger don't do any actual preemption but record
    // that preemption occoured while in the debugger.
    isolate->debug()->PreemptionWhileInDebugger();
  } else {
    // Perform preemption.
    v8::Unlocker unlocker;
    Thread::YieldCPU();
  }
#else
  { // NOLINT
    // Perform preemption.
    v8::Unlocker unlocker;
    Thread::YieldCPU();
  }
#endif

  return isolate->heap()->undefined_value();
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Object* Execution::DebugBreakHelper() {
  Isolate* isolate = Isolate::Current();

  // Just continue if breaks are disabled.
  if (isolate->debug()->disable_break()) {
    return isolate->heap()->undefined_value();
  }

  // Ignore debug break during bootstrapping.
  if (isolate->bootstrapper()->IsActive()) {
    return isolate->heap()->undefined_value();
  }

  {
    JavaScriptFrameIterator it;
    ASSERT(!it.done());
    Object* fun = it.frame()->function();
    if (fun && fun->IsJSFunction()) {
      // Don't stop in builtin functions.
      if (JSFunction::cast(fun)->IsBuiltin()) {
        return isolate->heap()->undefined_value();
      }
      GlobalObject* global = JSFunction::cast(fun)->context()->global();
      // Don't stop in debugger functions.
      if (isolate->debug()->IsDebugGlobal(global)) {
        return isolate->heap()->undefined_value();
      }
    }
  }

  // Collect the break state before clearing the flags.
  bool debug_command_only =
      isolate->stack_guard()->IsDebugCommand() &&
      !isolate->stack_guard()->IsDebugBreak();

  // Clear the debug break request flag.
  isolate->stack_guard()->Continue(DEBUGBREAK);

  ProcessDebugMesssages(debug_command_only);

  // Return to continue execution.
  return isolate->heap()->undefined_value();
}

void Execution::ProcessDebugMesssages(bool debug_command_only) {
  // Clear the debug command request flag.
  Isolate::Current()->stack_guard()->Continue(DEBUGCOMMAND);

  HandleScope scope;
  // Enter the debugger. Just continue if we fail to enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) {
    return;
  }

  // Notify the debug event listeners. Indicate auto continue if the break was
  // a debug command break.
  Isolate::Current()->debugger()->OnDebugBreak(FACTORY->undefined_value(),
                                               debug_command_only);
}


#endif

MaybeObject* Execution::HandleStackGuardInterrupt() {
  Isolate* isolate = Isolate::Current();
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (isolate->stack_guard()->IsDebugBreak() ||
      isolate->stack_guard()->IsDebugCommand()) {
    DebugBreakHelper();
  }
#endif
  if (isolate->stack_guard()->IsPreempted()) RuntimePreempt();
  if (isolate->stack_guard()->IsTerminateExecution()) {
    isolate->stack_guard()->Continue(TERMINATE);
    return isolate->TerminateExecution();
  }
  if (isolate->stack_guard()->IsInterrupted()) {
    // interrupt
    isolate->stack_guard()->Continue(INTERRUPT);
    return isolate->StackOverflow();
  }
  return isolate->heap()->undefined_value();
}

} }  // namespace v8::internal
