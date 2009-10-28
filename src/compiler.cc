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

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "compilation-cache.h"
#include "compiler.h"
#include "debug.h"
#include "fast-codegen.h"
#include "oprofile-agent.h"
#include "rewriter.h"
#include "scopes.h"
#include "usage-analyzer.h"

namespace v8 {
namespace internal {


class CodeGenSelector: public AstVisitor {
 public:
  enum CodeGenTag { NORMAL, FAST };

  CodeGenSelector()
      : has_supported_syntax_(true),
        location_(Location::Nowhere()) {
  }

  CodeGenTag Select(FunctionLiteral* fun);

 private:
  void VisitDeclarations(ZoneList<Declaration*>* decls);
  void VisitStatements(ZoneList<Statement*>* stmts);

  // Visit an expression in effect context with a desired location of
  // nowhere.
  void VisitAsEffect(Expression* expr);

  // Visit an expression in value context with a desired location of
  // temporary.
  void VisitAsValue(Expression* expr);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  bool has_supported_syntax_;

  // The desired location of the currently visited expression.
  Location location_;

  DISALLOW_COPY_AND_ASSIGN(CodeGenSelector);
};


static Handle<Code> MakeCode(FunctionLiteral* literal,
                             Handle<Script> script,
                             Handle<Context> context,
                             bool is_eval) {
  ASSERT(literal != NULL);

  // Rewrite the AST by introducing .result assignments where needed.
  if (!Rewriter::Process(literal) || !AnalyzeVariableUsage(literal)) {
    // Signal a stack overflow by returning a null handle.  The stack
    // overflow exception will be thrown by the caller.
    return Handle<Code>::null();
  }

  {
    // Compute top scope and allocate variables. For lazy compilation
    // the top scope only contains the single lazily compiled function,
    // so this doesn't re-allocate variables repeatedly.
    HistogramTimerScope timer(&Counters::variable_allocation);
    Scope* top = literal->scope();
    while (top->outer_scope() != NULL) top = top->outer_scope();
    top->AllocateVariables(context);
  }

#ifdef DEBUG
  if (Bootstrapper::IsActive() ?
      FLAG_print_builtin_scopes :
      FLAG_print_scopes) {
    literal->scope()->Print();
  }
#endif

  // Optimize the AST.
  if (!Rewriter::Optimize(literal)) {
    // Signal a stack overflow by returning a null handle.  The stack
    // overflow exception will be thrown by the caller.
    return Handle<Code>::null();
  }

  // Generate code and return it.
  if (FLAG_fast_compiler) {
    CodeGenSelector selector;
    CodeGenSelector::CodeGenTag code_gen = selector.Select(literal);
    if (code_gen == CodeGenSelector::FAST) {
      return FastCodeGenerator::MakeCode(literal, script, is_eval);
    }
    ASSERT(code_gen == CodeGenSelector::NORMAL);
  }
  return CodeGenerator::MakeCode(literal, script, is_eval);
}


static bool IsValidJSON(FunctionLiteral* lit) {
  if (lit->body()->length() != 1)
    return false;
  Statement* stmt = lit->body()->at(0);
  if (stmt->AsExpressionStatement() == NULL)
    return false;
  Expression* expr = stmt->AsExpressionStatement()->expression();
  return expr->IsValidJSON();
}


static Handle<JSFunction> MakeFunction(bool is_global,
                                       bool is_eval,
                                       Compiler::ValidationState validate,
                                       Handle<Script> script,
                                       Handle<Context> context,
                                       v8::Extension* extension,
                                       ScriptDataImpl* pre_data) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  PostponeInterruptsScope postpone;

  ASSERT(!i::Top::global_context().is_null());
  script->set_context_data((*i::Top::global_context())->data());

#ifdef ENABLE_DEBUGGER_SUPPORT
  bool is_json = (validate == Compiler::VALIDATE_JSON);
  if (is_eval || is_json) {
    script->set_compilation_type(
        is_json ? Smi::FromInt(Script::COMPILATION_TYPE_JSON) :
                               Smi::FromInt(Script::COMPILATION_TYPE_EVAL));
    // For eval scripts add information on the function from which eval was
    // called.
    if (is_eval) {
      JavaScriptFrameIterator it;
      script->set_eval_from_function(it.frame()->function());
      int offset = it.frame()->pc() - it.frame()->code()->instruction_start();
      script->set_eval_from_instructions_offset(Smi::FromInt(offset));
    }
  }

  // Notify debugger
  Debugger::OnBeforeCompile(script);
#endif

  // Only allow non-global compiles for eval.
  ASSERT(is_eval || is_global);

  // Build AST.
  FunctionLiteral* lit = MakeAST(is_global, script, extension, pre_data);

  // Check for parse errors.
  if (lit == NULL) {
    ASSERT(Top::has_pending_exception());
    return Handle<JSFunction>::null();
  }

  // When parsing JSON we do an ordinary parse and then afterwards
  // check the AST to ensure it was well-formed.  If not we give a
  // syntax error.
  if (validate == Compiler::VALIDATE_JSON && !IsValidJSON(lit)) {
    HandleScope scope;
    Handle<JSArray> args = Factory::NewJSArray(1);
    Handle<Object> source(script->source());
    SetElement(args, 0, source);
    Handle<Object> result = Factory::NewSyntaxError("invalid_json", args);
    Top::Throw(*result, NULL);
    return Handle<JSFunction>::null();
  }

  // Measure how long it takes to do the compilation; only take the
  // rest of the function into account to avoid overlap with the
  // parsing statistics.
  HistogramTimer* rate = is_eval
      ? &Counters::compile_eval
      : &Counters::compile;
  HistogramTimerScope timer(rate);

  // Compile the code.
  Handle<Code> code = MakeCode(lit, script, context, is_eval);

  // Check for stack-overflow exceptions.
  if (code.is_null()) {
    Top::StackOverflow();
    return Handle<JSFunction>::null();
  }

#if defined ENABLE_LOGGING_AND_PROFILING || defined ENABLE_OPROFILE_AGENT
  // Log the code generation for the script. Check explicit whether logging is
  // to avoid allocating when not required.
  if (Logger::is_logging() || OProfileAgent::is_enabled()) {
    if (script->name()->IsString()) {
      SmartPointer<char> data =
          String::cast(script->name())->ToCString(DISALLOW_NULLS);
      LOG(CodeCreateEvent(is_eval ? Logger::EVAL_TAG : Logger::SCRIPT_TAG,
                          *code, *data));
      OProfileAgent::CreateNativeCodeRegion(*data,
                                            code->instruction_start(),
                                            code->instruction_size());
    } else {
      LOG(CodeCreateEvent(is_eval ? Logger::EVAL_TAG : Logger::SCRIPT_TAG,
                          *code, ""));
      OProfileAgent::CreateNativeCodeRegion(is_eval ? "Eval" : "Script",
                                            code->instruction_start(),
                                            code->instruction_size());
    }
  }
#endif

  // Allocate function.
  Handle<JSFunction> fun =
      Factory::NewFunctionBoilerplate(lit->name(),
                                      lit->materialized_literal_count(),
                                      code);

  ASSERT_EQ(RelocInfo::kNoPosition, lit->function_token_position());
  CodeGenerator::SetFunctionInfo(fun, lit, true, script);

  // Hint to the runtime system used when allocating space for initial
  // property space by setting the expected number of properties for
  // the instances of the function.
  SetExpectedNofPropertiesFromEstimate(fun, lit->expected_property_count());

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger
  Debugger::OnAfterCompile(script, fun);
#endif

  return fun;
}


static StaticResource<SafeStringInputBuffer> safe_string_input_buffer;


Handle<JSFunction> Compiler::Compile(Handle<String> source,
                                     Handle<Object> script_name,
                                     int line_offset, int column_offset,
                                     v8::Extension* extension,
                                     ScriptDataImpl* input_pre_data) {
  int source_length = source->length();
  Counters::total_load_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache but not for extensions.
  Handle<JSFunction> result;
  if (extension == NULL) {
    result = CompilationCache::LookupScript(source,
                                            script_name,
                                            line_offset,
                                            column_offset);
  }

  if (result.is_null()) {
    // No cache entry found. Do pre-parsing and compile the script.
    ScriptDataImpl* pre_data = input_pre_data;
    if (pre_data == NULL && source_length >= FLAG_min_preparse_length) {
      Access<SafeStringInputBuffer> buf(&safe_string_input_buffer);
      buf->Reset(source.location());
      pre_data = PreParse(source, buf.value(), extension);
    }

    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(Smi::FromInt(line_offset));
      script->set_column_offset(Smi::FromInt(column_offset));
    }

    // Compile the function and add it to the cache.
    result = MakeFunction(true,
                          false,
                          DONT_VALIDATE_JSON,
                          script,
                          Handle<Context>::null(),
                          extension,
                          pre_data);
    if (extension == NULL && !result.is_null()) {
      CompilationCache::PutScript(source, result);
    }

    // Get rid of the pre-parsing data (if necessary).
    if (input_pre_data == NULL && pre_data != NULL) {
      delete pre_data;
    }
  }

  if (result.is_null()) Top::ReportPendingMessages();
  return result;
}


Handle<JSFunction> Compiler::CompileEval(Handle<String> source,
                                         Handle<Context> context,
                                         bool is_global,
                                         ValidationState validate) {
  // Note that if validation is required then no path through this
  // function is allowed to return a value without validating that
  // the input is legal json.

  int source_length = source->length();
  Counters::total_eval_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache; if the entry is not there,
  // invoke the compiler and add the result to the cache.  If we're
  // evaluating json we bypass the cache since we can't be sure a
  // potential value in the cache has been validated.
  Handle<JSFunction> result;
  if (validate == DONT_VALIDATE_JSON)
    result = CompilationCache::LookupEval(source, context, is_global);

  if (result.is_null()) {
    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    result = MakeFunction(is_global,
                          true,
                          validate,
                          script,
                          context,
                          NULL,
                          NULL);
    if (!result.is_null() && validate != VALIDATE_JSON) {
      // For json it's unlikely that we'll ever see exactly the same
      // string again so we don't use the compilation cache.
      CompilationCache::PutEval(source, context, is_global, result);
    }
  }

  return result;
}


bool Compiler::CompileLazy(Handle<SharedFunctionInfo> shared,
                           int loop_nesting) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  PostponeInterruptsScope postpone;

  // Compute name, source code and script data.
  Handle<String> name(String::cast(shared->name()));
  Handle<Script> script(Script::cast(shared->script()));

  int start_position = shared->start_position();
  int end_position = shared->end_position();
  bool is_expression = shared->is_expression();
  Counters::total_compile_size.Increment(end_position - start_position);

  // Generate the AST for the lazily compiled function. The AST may be
  // NULL in case of parser stack overflow.
  FunctionLiteral* lit = MakeLazyAST(script, name,
                                     start_position,
                                     end_position,
                                     is_expression);

  // Check for parse errors.
  if (lit == NULL) {
    ASSERT(Top::has_pending_exception());
    return false;
  }

  // Update the loop nesting in the function literal.
  lit->set_loop_nesting(loop_nesting);

  // Measure how long it takes to do the lazy compilation; only take
  // the rest of the function into account to avoid overlap with the
  // lazy parsing statistics.
  HistogramTimerScope timer(&Counters::compile_lazy);

  // Compile the code.
  Handle<Code> code = MakeCode(lit, script, Handle<Context>::null(), false);

  // Check for stack-overflow exception.
  if (code.is_null()) {
    Top::StackOverflow();
    return false;
  }

#if defined ENABLE_LOGGING_AND_PROFILING || defined ENABLE_OPROFILE_AGENT
  // Log the code generation. If source information is available include script
  // name and line number. Check explicit whether logging is enabled as finding
  // the line number is not for free.
  if (Logger::is_logging() || OProfileAgent::is_enabled()) {
    Handle<String> func_name(name->length() > 0 ?
                             *name : shared->inferred_name());
    if (script->name()->IsString()) {
      int line_num = GetScriptLineNumber(script, start_position) + 1;
      LOG(CodeCreateEvent(Logger::LAZY_COMPILE_TAG, *code, *func_name,
                          String::cast(script->name()), line_num));
      OProfileAgent::CreateNativeCodeRegion(*func_name,
                                            String::cast(script->name()),
                                            line_num,
                                            code->instruction_start(),
                                            code->instruction_size());
    } else {
      LOG(CodeCreateEvent(Logger::LAZY_COMPILE_TAG, *code, *func_name));
      OProfileAgent::CreateNativeCodeRegion(*func_name,
                                            code->instruction_start(),
                                            code->instruction_size());
    }
  }
#endif

  // Update the shared function info with the compiled code.
  shared->set_code(*code);

  // Set the expected number of properties for instances.
  SetExpectedNofPropertiesFromEstimate(shared, lit->expected_property_count());

  // Set the optimication hints after performing lazy compilation, as these are
  // not set when the function is set up as a lazily compiled function.
  shared->SetThisPropertyAssignmentsInfo(
      lit->has_only_this_property_assignments(),
      lit->has_only_simple_this_property_assignments(),
      *lit->this_property_assignments());

  // Check the function has compiled code.
  ASSERT(shared->is_compiled());
  return true;
}


CodeGenSelector::CodeGenTag CodeGenSelector::Select(FunctionLiteral* fun) {
  Scope* scope = fun->scope();

  if (!scope->is_global_scope()) {
    if (FLAG_trace_bailout) PrintF("Non-global scope\n");
    return NORMAL;
  }
  ASSERT(scope->num_heap_slots() == 0);
  ASSERT(scope->arguments() == NULL);

  has_supported_syntax_ = true;
  VisitDeclarations(fun->scope()->declarations());
  if (!has_supported_syntax_) return NORMAL;

  VisitStatements(fun->body());
  return has_supported_syntax_ ? FAST : NORMAL;
}


#define BAILOUT(reason)                         \
  do {                                          \
    if (FLAG_trace_bailout) {                   \
      PrintF("%s\n", reason);                   \
    }                                           \
    has_supported_syntax_ = false;              \
    return;                                     \
  } while (false)


#define CHECK_BAILOUT                           \
  do {                                          \
    if (!has_supported_syntax_) return;         \
  } while (false)


void CodeGenSelector::VisitDeclarations(ZoneList<Declaration*>* decls) {
  for (int i = 0; i < decls->length(); i++) {
    Visit(decls->at(i));
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0, len = stmts->length(); i < len; i++) {
    Visit(stmts->at(i));
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitAsEffect(Expression* expr) {
  if (location_.is_nowhere()) {
    Visit(expr);
  } else {
    Location saved = location_;
    location_ = Location::Nowhere();
    Visit(expr);
    location_ = saved;
  }
}


void CodeGenSelector::VisitAsValue(Expression* expr) {
  if (location_.is_temporary()) {
    Visit(expr);
  } else {
    Location saved = location_;
    location_ = Location::Temporary();
    Visit(expr);
    location_ = saved;
  }
}


void CodeGenSelector::VisitDeclaration(Declaration* decl) {
  Variable* var = decl->proxy()->var();
  if (!var->is_global() || var->mode() == Variable::CONST) {
    BAILOUT("Non-global declaration");
  }
}


void CodeGenSelector::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void CodeGenSelector::VisitExpressionStatement(ExpressionStatement* stmt) {
  VisitAsEffect(stmt->expression());
}


void CodeGenSelector::VisitEmptyStatement(EmptyStatement* stmt) {
  // EmptyStatement is supported.
}


void CodeGenSelector::VisitIfStatement(IfStatement* stmt) {
  BAILOUT("IfStatement");
}


void CodeGenSelector::VisitContinueStatement(ContinueStatement* stmt) {
  BAILOUT("ContinueStatement");
}


void CodeGenSelector::VisitBreakStatement(BreakStatement* stmt) {
  BAILOUT("BreakStatement");
}


void CodeGenSelector::VisitReturnStatement(ReturnStatement* stmt) {
  VisitAsValue(stmt->expression());
}


void CodeGenSelector::VisitWithEnterStatement(WithEnterStatement* stmt) {
  BAILOUT("WithEnterStatement");
}


void CodeGenSelector::VisitWithExitStatement(WithExitStatement* stmt) {
  BAILOUT("WithExitStatement");
}


void CodeGenSelector::VisitSwitchStatement(SwitchStatement* stmt) {
  BAILOUT("SwitchStatement");
}


void CodeGenSelector::VisitDoWhileStatement(DoWhileStatement* stmt) {
  BAILOUT("DoWhileStatement");
}


void CodeGenSelector::VisitWhileStatement(WhileStatement* stmt) {
  BAILOUT("WhileStatement");
}


void CodeGenSelector::VisitForStatement(ForStatement* stmt) {
  BAILOUT("ForStatement");
}


void CodeGenSelector::VisitForInStatement(ForInStatement* stmt) {
  BAILOUT("ForInStatement");
}


void CodeGenSelector::VisitTryCatchStatement(TryCatchStatement* stmt) {
  BAILOUT("TryCatchStatement");
}


void CodeGenSelector::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  BAILOUT("TryFinallyStatement");
}


void CodeGenSelector::VisitDebuggerStatement(DebuggerStatement* stmt) {
  BAILOUT("DebuggerStatement");
}


void CodeGenSelector::VisitFunctionLiteral(FunctionLiteral* expr) {
  if (!expr->AllowsLazyCompilation()) {
    BAILOUT("FunctionLiteral does not allow lazy compilation");
  }
  expr->set_location(location_);
}


void CodeGenSelector::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  BAILOUT("FunctionBoilerplateLiteral");
}


void CodeGenSelector::VisitConditional(Conditional* expr) {
  BAILOUT("Conditional");
}


void CodeGenSelector::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void CodeGenSelector::VisitVariableProxy(VariableProxy* expr) {
  Expression* rewrite = expr->var()->rewrite();
  // A rewrite of NULL indicates a global variable.
  if (rewrite != NULL) {
    // Non-global.
    Slot* slot = rewrite->AsSlot();
    if (slot == NULL) {
      // This is a variable rewritten to an explicit property access
      // on the arguments object.
      BAILOUT("non-global/non-slot variable reference");
    }

    Slot::Type type = slot->type();
    if (type != Slot::PARAMETER && type != Slot::LOCAL) {
      BAILOUT("non-parameter/non-local slot reference");
    }
  }
  expr->set_location(location_);
}


void CodeGenSelector::VisitLiteral(Literal* expr) {
  expr->set_location(location_);
}


void CodeGenSelector::VisitRegExpLiteral(RegExpLiteral* expr) {
  expr->set_location(location_);
}


void CodeGenSelector::VisitObjectLiteral(ObjectLiteral* expr) {
  ZoneList<ObjectLiteral::Property*>* properties = expr->properties();

  for (int i = 0, len = properties->length(); i < len; i++) {
    ObjectLiteral::Property* property = properties->at(i);
    if (property->IsCompileTimeValue()) continue;

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();

      // For (non-compile-time) materialized literals and computed
      // properties with symbolic keys we will use an IC and therefore not
      // generate code for the key.
      case ObjectLiteral::Property::COMPUTED:  // Fall through.
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        if (property->key()->handle()->IsSymbol()) {
          break;
        }
        // Fall through.

      // In all other cases we need the key's value on the stack
      // for a runtime call.  (Relies on TEMP meaning STACK.)
      case ObjectLiteral::Property::GETTER:  // Fall through.
      case ObjectLiteral::Property::SETTER:  // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
        VisitAsValue(property->key());
        CHECK_BAILOUT;
        break;
    }
    VisitAsValue(property->value());
    CHECK_BAILOUT;
  }
  expr->set_location(location_);
}


void CodeGenSelector::VisitArrayLiteral(ArrayLiteral* expr) {
  ZoneList<Expression*>* subexprs = expr->values();
  for (int i = 0, len = subexprs->length(); i < len; i++) {
    Expression* subexpr = subexprs->at(i);
    if (subexpr->AsLiteral() != NULL) continue;
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;
    VisitAsValue(subexpr);
    CHECK_BAILOUT;
  }
  expr->set_location(location_);
}


void CodeGenSelector::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  BAILOUT("CatchExtensionObject");
}


void CodeGenSelector::VisitAssignment(Assignment* expr) {
  // We support plain non-compound assignments to parameters and
  // non-context (stack-allocated) locals.
  if (expr->starts_initialization_block() ||
      expr->ends_initialization_block()) {
    BAILOUT("initialization block start");
  }

  Token::Value op = expr->op();
  if (op == Token::INIT_CONST) BAILOUT("initialize constant");
  if (op != Token::ASSIGN && op != Token::INIT_VAR) {
    BAILOUT("compound assignment");
  }

  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  if (var == NULL) BAILOUT("non-variable assignment");

  if (!var->is_global()) {
    ASSERT(var->slot() != NULL);
    Slot::Type type = var->slot()->type();
    if (type != Slot::PARAMETER && type != Slot::LOCAL) {
      BAILOUT("non-parameter/non-local slot assignment");
    }
  }

  VisitAsValue(expr->value());
  expr->set_location(location_);
}


void CodeGenSelector::VisitThrow(Throw* expr) {
  BAILOUT("Throw");
}


void CodeGenSelector::VisitProperty(Property* expr) {
  VisitAsValue(expr->obj());
  CHECK_BAILOUT;
  VisitAsValue(expr->key());
  expr->set_location(location_);
}


void CodeGenSelector::VisitCall(Call* expr) {
  Expression* fun = expr->expression();
  ZoneList<Expression*>* args = expr->arguments();
  Variable* var = fun->AsVariableProxy()->AsVariable();

  // Check for supported calls
  if (var != NULL && var->is_possibly_eval()) {
    BAILOUT("Call to a function named 'eval'");
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // ----------------------------------
    // JavaScript example: 'foo(1, 2, 3)'  // foo is global
    // ----------------------------------
  } else {
    BAILOUT("Call to a non-global function");
  }
  // Check all arguments to the call.  (Relies on TEMP meaning STACK.)
  for (int i = 0; i < args->length(); i++) {
    VisitAsValue(args->at(i));
    CHECK_BAILOUT;
  }
  expr->set_location(location_);
}


void CodeGenSelector::VisitCallNew(CallNew* expr) {
  VisitAsValue(expr->expression());
  CHECK_BAILOUT;
  ZoneList<Expression*>* args = expr->arguments();
  // Check all arguments to the call
  for (int i = 0; i < args->length(); i++) {
    VisitAsValue(args->at(i));
    CHECK_BAILOUT;
  }
  expr->set_location(location_);
}


void CodeGenSelector::VisitCallRuntime(CallRuntime* expr) {
  // In case of JS runtime function bail out.
  if (expr->function() == NULL) BAILOUT("call JS runtime function");
  // Check for inline runtime call
  if (expr->name()->Get(0) == '_' &&
      CodeGenerator::FindInlineRuntimeLUT(expr->name()) != NULL) {
    BAILOUT("inlined runtime call");
  }
  // Check all arguments to the call.  (Relies on TEMP meaning STACK.)
  for (int i = 0; i < expr->arguments()->length(); i++) {
    VisitAsValue(expr->arguments()->at(i));
    CHECK_BAILOUT;
  }
  expr->set_location(location_);
}


void CodeGenSelector::VisitUnaryOperation(UnaryOperation* expr) {
  BAILOUT("UnaryOperation");
}


void CodeGenSelector::VisitCountOperation(CountOperation* expr) {
  BAILOUT("CountOperation");
}


void CodeGenSelector::VisitBinaryOperation(BinaryOperation* expr) {
  switch (expr->op()) {
    case Token::OR:
      VisitAsValue(expr->left());
      CHECK_BAILOUT;
      // The location for the right subexpression is the same as for the
      // whole expression so we call Visit directly.
      Visit(expr->right());
      break;

    default:
      BAILOUT("Unsupported binary operation");
  }
  expr->set_location(location_);
}


void CodeGenSelector::VisitCompareOperation(CompareOperation* expr) {
  BAILOUT("CompareOperation");
}


void CodeGenSelector::VisitThisFunction(ThisFunction* expr) {
  BAILOUT("ThisFunction");
}

#undef BAILOUT
#undef CHECK_BAILOUT


} }  // namespace v8::internal
