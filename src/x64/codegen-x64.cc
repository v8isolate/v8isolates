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
#include "macro-assembler.h"
#include "register-allocator-inl.h"
#include "codegen.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// Platform-specific DeferredCode functions.

void DeferredCode::SaveRegisters() { UNIMPLEMENTED(); }

void DeferredCode::RestoreRegisters() { UNIMPLEMENTED(); }

// -------------------------------------------------------------------------
// CodeGenState implementation.

CodeGenState::CodeGenState(CodeGenerator* owner)
    : owner_(owner),
      typeof_state_(NOT_INSIDE_TYPEOF),
      destination_(NULL),
      previous_(NULL) {
  owner_->set_state(this);
}


CodeGenState::CodeGenState(CodeGenerator* owner,
                           TypeofState typeof_state,
                           ControlDestination* destination)
    : owner_(owner),
      typeof_state_(typeof_state),
      destination_(destination),
      previous_(owner->state()) {
  owner_->set_state(this);
}


CodeGenState::~CodeGenState() {
  ASSERT(owner_->state() == this);
  owner_->set_state(previous_);
}


// -----------------------------------------------------------------------------
// CodeGenerator implementation.

CodeGenerator::CodeGenerator(int buffer_size,
                             Handle<Script> script,
                             bool is_eval)
    : is_eval_(is_eval),
      script_(script),
      deferred_(8),
      masm_(new MacroAssembler(NULL, buffer_size)),
      scope_(NULL),
      frame_(NULL),
      allocator_(NULL),
      state_(NULL),
      loop_nesting_(0),
      function_return_is_shadowed_(false),
      in_spilled_code_(false) {
}

#define __ ACCESS_MASM(masm_)


void CodeGenerator::DeclareGlobals(Handle<FixedArray> a) {
  UNIMPLEMENTED();
}

void CodeGenerator::TestCodeGenerator() {
  // Generate code.
  ZoneScope zone_scope(DELETE_ON_EXIT);
  const int initial_buffer_size = 4 * KB;
  CodeGenerator cgen(initial_buffer_size, NULL, false);
  CodeGeneratorScope scope(&cgen);
  Scope dummy_scope;
  cgen.scope_ = &dummy_scope;
  cgen.GenCode(NULL);

  CodeDesc desc;
  cgen.masm()->GetCode(&desc);
}


void CodeGenerator::GenCode(FunctionLiteral* function) {
  if (function != NULL) {
    CodeForFunctionPosition(function);
    // ASSERT(scope_ == NULL);
    // scope_ = function->scope();
    __ int3();  // UNIMPLEMENTED
  } else {
    // GenCode Implementation under construction.  Run by TestCodeGenerator
    // with a == NULL.
    // Everything guarded by if (function) should be run, unguarded,
    // once testing is over.
    // Record the position for debugging purposes.
    if (function) {
      CodeForFunctionPosition(function);
    }
    // ZoneList<Statement*>* body = fun->body();

    // Initialize state.
    // While testing, scope is set in cgen before calling GenCode().
    if (function) {
      ASSERT(scope_ == NULL);
      scope_ = function->scope();
    }
    ASSERT(allocator_ == NULL);
    RegisterAllocator register_allocator(this);
    allocator_ = &register_allocator;
    ASSERT(frame_ == NULL);
    frame_ = new VirtualFrame();
    set_in_spilled_code(false);

    // Adjust for function-level loop nesting.
    // loop_nesting_ += fun->loop_nesting();

    JumpTarget::set_compiling_deferred_code(false);

#ifdef DEBUG
    if (strlen(FLAG_stop_at) > 0 &&
        //    fun->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
        false) {
      frame_->SpillAll();
      __ int3();
    }
#endif

    // New scope to get automatic timing calculation.
    {  // NOLINT
      HistogramTimerScope codegen_timer(&Counters::code_generation);
      CodeGenState state(this);

      // Entry:
      // Stack: receiver, arguments, return address.
      // ebp: caller's frame pointer
      // esp: stack pointer
      // edi: called JS function
      // esi: callee's context
      allocator_->Initialize();
      frame_->Enter();

      __ movq(rax, Immediate(0x2a));
      __ Ret();
    }
  }
}

void CodeGenerator::GenerateFastCaseSwitchJumpTable(SwitchStatement* a,
                                                    int b,
                                                    int c,
                                                    Label* d,
                                                    Vector<Label*> e,
                                                    Vector<Label> f) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitStatements(ZoneList<Statement*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitBlock(Block* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitDeclaration(Declaration* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitExpressionStatement(ExpressionStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitEmptyStatement(EmptyStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitIfStatement(IfStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitContinueStatement(ContinueStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitBreakStatement(BreakStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitReturnStatement(ReturnStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitWithEnterStatement(WithEnterStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitWithExitStatement(WithExitStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitSwitchStatement(SwitchStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitLoopStatement(LoopStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitForInStatement(ForInStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitTryCatch(TryCatch* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitTryFinally(TryFinally* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitDebuggerStatement(DebuggerStatement* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitConditional(Conditional* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitSlot(Slot* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitVariableProxy(VariableProxy* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitLiteral(Literal* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitRegExpLiteral(RegExpLiteral* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitObjectLiteral(ObjectLiteral* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitArrayLiteral(ArrayLiteral* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitAssignment(Assignment* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitThrow(Throw* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitProperty(Property* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitCall(Call* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitCallEval(CallEval* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitCallNew(CallNew* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitCallRuntime(CallRuntime* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitUnaryOperation(UnaryOperation* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitCountOperation(CountOperation* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitBinaryOperation(BinaryOperation* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitCompareOperation(CompareOperation* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::VisitThisFunction(ThisFunction* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateArgumentsAccess(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateArgumentsLength(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateFastCharCodeAt(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateIsArray(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateIsNonNegativeSmi(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateIsSmi(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateLog(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateObjectEquals(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateSetValueOf(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

void CodeGenerator::GenerateValueOf(ZoneList<Expression*>* a) {
  UNIMPLEMENTED();
}

#undef __
// End of CodeGenerator implementation.

// -----------------------------------------------------------------------------
// Implementation of stubs.

//  Stub classes have public member named masm, not masm_.
#define __ ACCESS_MASM(masm)

void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  // Check that stack should contain frame pointer, code pointer, state and
  // return address in that order.
  ASSERT_EQ(StackHandlerConstants::kFPOffset + kPointerSize,
            StackHandlerConstants::kStateOffset);
  ASSERT_EQ(StackHandlerConstants::kStateOffset + kPointerSize,
            StackHandlerConstants::kPCOffset);

  ExternalReference handler_address(Top::k_handler_address);
  __ movq(kScratchRegister, handler_address);
  __ movq(rdx, Operand(kScratchRegister, 0));
  // get next in chain
  __ movq(rcx, Operand(rdx, 0));
  __ movq(Operand(kScratchRegister, 0), rcx);
  __ movq(rsp, rdx);
  __ pop(rbp);  // pop frame pointer
  __ pop(rdx);  // remove code pointer
  __ pop(rdx);  // remove state

  // Before returning we restore the context from the frame pointer if not NULL.
  // The frame pointer is NULL in the exception handler of a JS entry frame.
  __ xor_(rsi, rsi);  // tentatively set context pointer to NULL
  Label skip;
  __ cmpq(rbp, Immediate(0));
  __ j(equal, &skip);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);

  __ ret(0);
}



void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_out_of_memory_exception,
                              StackFrame::Type frame_type,
                              bool do_gc,
                              bool always_allocate_scope) {
  // rax: result parameter for PerformGC, if any
  // rbx: pointer to C function  (C callee-saved)
  // rbp: frame pointer  (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rdi: number of arguments including receiver  (C callee-saved)
  // rsi: pointer to the first argument (C callee-saved)

  if (do_gc) {
    __ movq(Operand(rsp, 0), rax);  // Result.
    __ movq(kScratchRegister,
            FUNCTION_ADDR(Runtime::PerformGC),
            RelocInfo::RUNTIME_ENTRY);
    __ call(kScratchRegister);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth();
  if (always_allocate_scope) {
    __ movq(kScratchRegister, scope_depth);
    __ incl(Operand(kScratchRegister, 0));
  }

  // Call C function.
#ifdef __MSVC__
  // MSVC passes arguments in rcx, rdx, r8, r9
  __ movq(rcx, rdi);  // argc.
  __ movq(rdx, rsi);  // argv.
#else  // ! defined(__MSVC__)
  // GCC passes arguments in rdi, rsi, rdx, rcx, r8, r9.
  // First two arguments are already in rdi, rsi.
#endif
  __ call(rbx);
  // Result is in rax - do not destroy this register!

  if (always_allocate_scope) {
    __ movq(kScratchRegister, scope_depth);
    __ decl(Operand(kScratchRegister, 0));
  }

  // Check for failure result.
  Label failure_returned;
  ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  __ lea(rcx, Operand(rax, 1));
  // Lower 2 bits of rcx are 0 iff rax has failure tag.
  __ testl(rcx, Immediate(kFailureTagMask));
  __ j(zero, &failure_returned);

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(frame_type);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ testq(rax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry);

  Label continue_exception;
  // If the returned failure is EXCEPTION then promote Top::pending_exception().
  __ movq(kScratchRegister, Failure::Exception(), RelocInfo::NONE);
  __ cmpq(rax, kScratchRegister);
  __ j(not_equal, &continue_exception);

  // Retrieve the pending exception and clear the variable.
  ExternalReference pending_exception_address(Top::k_pending_exception_address);
  __ movq(kScratchRegister, pending_exception_address);
  __ movq(rax, Operand(kScratchRegister, 0));
  __ movq(rdx, ExternalReference::the_hole_value_location());
  __ movq(rdx, Operand(rdx, 0));
  __ movq(Operand(kScratchRegister, 0), rdx);

  __ bind(&continue_exception);
  // Special handling of out of memory exception.
  __ movq(kScratchRegister, Failure::OutOfMemoryException(), RelocInfo::NONE);
  __ cmpq(rax, kScratchRegister);
  __ j(equal, throw_out_of_memory_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::GenerateThrowOutOfMemory(MacroAssembler* masm) {
  // Fetch top stack handler.
  ExternalReference handler_address(Top::k_handler_address);
  __ movq(kScratchRegister, handler_address);
  __ movq(rdx, Operand(kScratchRegister, 0));

  // Unwind the handlers until the ENTRY handler is found.
  Label loop, done;
  __ bind(&loop);
  // Load the type of the current stack handler.
  __ cmpq(Operand(rdx, StackHandlerConstants::kStateOffset),
         Immediate(StackHandler::ENTRY));
  __ j(equal, &done);
  // Fetch the next handler in the list.
  __ movq(rdx, Operand(rdx, StackHandlerConstants::kNextOffset));
  __ jmp(&loop);
  __ bind(&done);

  // Set the top handler address to next handler past the current ENTRY handler.
  __ movq(rax, Operand(rdx, StackHandlerConstants::kNextOffset));
  __ store_rax(handler_address);

  // Set external caught exception to false.
  __ movq(rax, Immediate(false));
  ExternalReference external_caught(Top::k_external_caught_exception_address);
  __ store_rax(external_caught);

  // Set pending exception and rax to out of memory exception.
  __ movq(rax, Failure::OutOfMemoryException(), RelocInfo::NONE);
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ store_rax(pending_exception);

  // Restore the stack to the address of the ENTRY handler
  __ movq(rsp, rdx);

  // Clear the context pointer;
  __ xor_(rsi, rsi);

  // Restore registers from handler.

  __ pop(rbp);  // FP
  ASSERT_EQ(StackHandlerConstants::kFPOffset + kPointerSize,
            StackHandlerConstants::kStateOffset);
  __ pop(rdx);  // State

  ASSERT_EQ(StackHandlerConstants::kStateOffset + kPointerSize,
            StackHandlerConstants::kPCOffset);
  __ ret(0);
}


void CEntryStub::GenerateBody(MacroAssembler* masm, bool is_debug_break) {
  // rax: number of arguments including receiver
  // rbx: pointer to C function  (C callee-saved)
  // rbp: frame pointer  (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rsi: current context (C callee-saved)
  // rdi: caller's parameter pointer pp  (C callee-saved)

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  StackFrame::Type frame_type = is_debug_break ?
      StackFrame::EXIT_DEBUG :
      StackFrame::EXIT;

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(frame_type);

  // rax: result parameter for PerformGC, if any (setup below)
  // rbx: pointer to builtin function  (C callee-saved)
  // rbp: frame pointer  (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rdi: number of arguments including receiver (C callee-saved)
  // rsi: argv pointer (C callee-saved)

  Label throw_out_of_memory_exception;
  Label throw_normal_exception;

  // Call into the runtime system. Collect garbage before the call if
  // running with --gc-greedy set.
  if (FLAG_gc_greedy) {
    Failure* failure = Failure::RetryAfterGC(0);
    __ movq(rax, failure, RelocInfo::NONE);
  }
  GenerateCore(masm, &throw_normal_exception,
               &throw_out_of_memory_exception,
               frame_type,
               FLAG_gc_greedy,
               false);

  // Do space-specific GC and retry runtime call.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_out_of_memory_exception,
               frame_type,
               true,
               false);

  // Do full GC and retry runtime call one final time.
  Failure* failure = Failure::InternalError();
  __ movq(rax, failure, RelocInfo::NONE);
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_out_of_memory_exception,
               frame_type,
               true,
               true);

  __ bind(&throw_out_of_memory_exception);
  GenerateThrowOutOfMemory(masm);
  // control flow for generated will not return.

  __ bind(&throw_normal_exception);
  GenerateThrowTOS(masm);
}


void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  Label invoke, exit;

  // Setup frame.
  __ push(rbp);
  __ movq(rbp, rsp);

  // Save callee-saved registers (X64 calling conventions).
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  // Push something that is not an arguments adaptor.
  __ push(Immediate(ArgumentsAdaptorFrame::NON_SENTINEL));
  __ push(Immediate(Smi::FromInt(marker)));  // @ function offset
  __ push(r12);
  __ push(r13);
  __ push(r14);
  __ push(r15);
  __ push(rdi);
  __ push(rsi);
  __ push(rbx);
  // TODO(X64): Push XMM6-XMM15 (low 64 bits) as well, or make them
  // callee-save in JS code as well.

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Top::k_c_entry_fp_address);
  __ load_rax(c_entry_fp);
  __ push(rax);

  // Call a faked try-block that does the invoke.
  __ call(&invoke);

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ store_rax(pending_exception);
  __ movq(rax, Failure::Exception(), RelocInfo::NONE);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);

  // Clear any pending exceptions.
  __ load_rax(ExternalReference::the_hole_value_location());
  __ store_rax(pending_exception);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline
  // builtin and pop the faked function when we return. We load the address
  // from an external reference instead of inlining the call target address
  // directly in the code, because the builtin stubs may not have been
  // generated yet at the time this code is generated.
  if (is_construct) {
    ExternalReference construct_entry(Builtins::JSConstructEntryTrampoline);
    __ load_rax(construct_entry);
  } else {
    ExternalReference entry(Builtins::JSEntryTrampoline);
    __ load_rax(entry);
  }
  __ lea(kScratchRegister, FieldOperand(rax, Code::kHeaderSize));
  __ call(kScratchRegister);

  // Unlink this frame from the handler chain.
  __ movq(kScratchRegister, ExternalReference(Top::k_handler_address));
  __ pop(Operand(kScratchRegister, 0));
  // Pop next_sp.
  __ addq(rsp, Immediate(StackHandlerConstants::kSize - kPointerSize));

  // Restore the top frame descriptor from the stack.
  __ bind(&exit);
  __ movq(kScratchRegister, ExternalReference(Top::k_c_entry_fp_address));
  __ pop(Operand(kScratchRegister, 0));

  // Restore callee-saved registers (X64 conventions).
  __ pop(rbx);
  __ pop(rsi);
  __ pop(rdi);
  __ pop(r15);
  __ pop(r14);
  __ pop(r13);
  __ pop(r12);
  __ addq(rsp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(rbp);
  __ ret(0);
}


#undef __

} }  // namespace v8::internal
