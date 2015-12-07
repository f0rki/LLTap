//===----------------- LLTap.cpp - Function Hooking Pass  -------*- C++ -*-===//
//
// Copyright 2015 Michael Rodler <contact@f0rki.at>
//
// This is free software, you can redistribute it and/or modify it under the
// terms of either:
//
// a) The Apache License, Version 2.0 as published by the Apache Foundation.
//    See LICENSE for details.
//
// b) The University of Illinois Open Source License, as used by the LLVM project.
//    See LICENSE-NCSA for details.
//
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
// THE SOFTWARE.
//
//===----------------------------------------------------------------------===//
///
/// \file LLTap.cpp
/// \todo write something here
///
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "LLTap"

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringSet.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/TypeBuilder.h"

#include "llvm/Pass.h"

#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/CommandLine.h"

#include <string>


using namespace std;
using namespace llvm;

//STATISTIC(FunctionsVisited, "# of functions that were visited.");
//STATISTIC(FunctionsInstrumented, "# of functions that were actually instrumented.");
//STATISTIC(CallsFound, "# calls to some function");
//STATISTIC(UsesFound, "# of other uses of functions");

namespace LLTap {

  enum InstrumentationMode {
    inst_ie,
    inst_i,
    inst_e,
  };

  enum class HookType {
    PRE_HOOK = 1,
    REPLACE_HOOK = 2,
    POST_HOOK = 4,
  };


  /**
   * then run this to instrument
   */
  class InstrumentationPass : public ModulePass {

    public:
      // Pass identification, replacement for typeid
      static char ID;

      InstrumentationPass() : ModulePass(ID) {}

      bool runOnModule(Module &M) override;

      //void getAnalysisUsage(AnalysisUsage &AU) const override {
      //}

    private:
      const string fn_lltap_get_hook = "__lltap_inst_get_hook";
      const string fn_lltap_add_hook = "__lltap_inst_add_hook_target";
      const string fn_lltap_has_hooks = "__lltap_inst_has_hooks";

      const string LLVM_GLOBAL_CTORS_VARNAME = "llvm.global_ctors";
      const int DEFAULT_CTOR_PRIORITY = 0;

      StringSet<> instrumentCallsTo;
      StringSet<> noInstrumentCallsTo;
      Regex* instrumentCallsRe = nullptr;
      Regex* noInstrumentCallsRe = nullptr;
      void initializeInstConfig();

      bool instConfigInitialized = false;
      bool shouldBeInstrumented(Function& F);
      bool runOnFunction(Function& F);

      bool instrumentCall(CallSite* inst, Module& M);
      bool instrumentUse(User* inst, Module& M);

      string mangleFunctionArgs(CallSite* CS);

      void addToGlobalCtors(Module& M, Function* fn);

      string getHookFunctionNameFor(Function* origFunc, CallSite* CS=nullptr);
      Function* getHookFunctionFor(CallSite* CS, Module& M);
      Function* getHookFunctionFor(Function* origFunc, Module& M);
      Function* createHookFunction(StringRef name, CallSite* call, Function* F, Module& M);
      Function* createHookFunction(StringRef name, Function* origFunc, Module& M);
      bool createHookingCode(Function* origFunc, Function* F, Module& M);

      void addCallTarget(Function* calledFn, Module &M);
      Function* getOrAddInitializerToModule(Module &M);
      void declareLLTapFunctions(Module &M);
      GlobalVariable* addFunctionNameAsStringConstant(Function* calledFn, Module &M);

  };
}

using namespace LLTap;

cl::OptionCategory LLTapCat("LLTap Options",
                            "These control how the LLTap pass instruments the calls.");

cl::opt<bool> DoNotInstrumentUses("no-inst-fptrs",
    cl::init(false),
    cl::desc("Turns off instrumentation of function pointer usage in the code."),
    cl::cat(LLTapCat));

cl::opt<InstrumentationMode> InstMode(cl::desc("Choose what calls LLTap instruments"),
    cl::init(InstrumentationMode::inst_ie),
    cl::values(
      clEnumVal(inst_i,
        "Instrument only 'internal' calls, which are defined in the same module."),
      clEnumVal(inst_e,
        "Instrument only 'external' calls, which are only declared in the module."),
      clEnumVal(inst_ie,
        "Instrument all types of calls"),
      clEnumValEnd),
    cl::cat(LLTapCat));

cl::list<string> InstrumentFunctions("inst-func",
    cl::desc("Pass name of function to instrument. (Use multiple times for more functions)"),
    cl::cat(LLTapCat));

cl::opt<string> InstrumentFunctionsRegexRaw("inst-funcs-re",
    cl::desc("Regex to match functions to be instrumented."),
    cl::cat(LLTapCat));

cl::list<string> NoInstrumentFunctions("no-inst-func",
    cl::desc("Pass name of function not to instrument. (Use multiple times for more functions)"),
    cl::cat(LLTapCat));

cl::opt<string> NoInstrumentFunctionsRegexRaw("no-inst-funcs-re",
    cl::desc("Regex to match functions not to be instrumented."),
    cl::cat(LLTapCat));


cl::opt<string> HookNamespace("hook-namespace",
    cl::desc("hook targets are registered using this namespace."),
    cl::cat(LLTapCat));


char LLTap::InstrumentationPass::ID = 0x42;
static RegisterPass<LLTap::InstrumentationPass> IP("LLTapInst", "LLTap instrumentation pass");


string LLTap::InstrumentationPass::mangleFunctionArgs(CallSite* CS) {
  //TODO: poor mans name mangling
  string mangled = "";
  DEBUG(dbgs() << "starting arg mangling\n");
  for (size_t i = 0; i < CS->getNumArgOperands(); ++i) {
    string typestr = "";
    Type* type = CS->getArgument(i)->getType();
    llvm::raw_string_ostream rso(typestr);
    type->print(rso);
    rso.flush();
    DEBUG(dbgs() << "typestring arg " << i << " " << typestr << " (should be " << *type << ")\n");
    Regex star("[\\*]");
    Regex whitespace("\\s");
    Regex printable("[^a-zA-Z0-9_]+");
    while (star.match(typestr)) {
      typestr = star.sub("p", typestr);
    }
    while (whitespace.match(typestr)) {
      typestr = whitespace.sub("_", typestr);
    }
    while (printable.match(typestr)) {
      typestr = printable.sub("", typestr);
    }
    DEBUG(dbgs() << "mangled type string " << typestr << "\n");
    mangled += typestr;
  }

  return mangled;
}

void LLTap::InstrumentationPass::declareLLTapFunctions(Module &M) {

  PointerType* i8ptr = PointerType::getUnqual(IntegerType::get(M.getContext(), 8));
  Type* i32 = IntegerType::get(M.getContext(), 32);
  //PointerType* voidptr = PointerType::getUnqual(Type::getVoidTy(M.getContext()));
  PointerType* voidptr = i8ptr;
  FunctionType *ft;
  std::vector<Type*> ftargs;

  // add function declarations to module:

  // void lltap_add_hook_target(void* addr, char* name);
  ftargs.clear();
  ftargs.push_back(voidptr);
  ftargs.push_back(i8ptr);
  ft = FunctionType::get(
      Type::getVoidTy(M.getContext()),
      ftargs,
      false);
  M.getOrInsertFunction(fn_lltap_add_hook, ft);

  // void* (void* addr, char* name);
  ftargs.clear();
  ftargs.push_back(voidptr);
  ftargs.push_back(i32);
  ft = FunctionType::get(
      voidptr,
      ftargs,
      false);
  M.getOrInsertFunction(fn_lltap_get_hook, ft);

  // int (void* addr);
  ftargs.clear();
  ftargs.push_back(voidptr);
  ft = FunctionType::get(
      i32,
      ftargs,
      false);
  M.getOrInsertFunction(fn_lltap_has_hooks, ft);
}


bool LLTap::InstrumentationPass::runOnModule(Module &M) {

  DEBUG(dbgs() << "running on module: " << M.getModuleIdentifier() << "\n");

  declareLLTapFunctions(M);

  // then instrument all the functions
  for (Function& F : M.getFunctionList()) {
    runOnFunction(F);
  }

  //DEBUG(dbgs() << "creating the following module" << M << "\n");

  return true;
}


void LLTap::InstrumentationPass::addToGlobalCtors(Module& M, Function* fn) {
  appendToGlobalCtors(M, fn, DEFAULT_CTOR_PRIORITY);
}


Function* LLTap::InstrumentationPass::getOrAddInitializerToModule(Module& M) {

  string name = "__lltap_init_";
  name.append(M.getName());

  Function* initFn = M.getFunction(name);

  if (initFn == NULL) {
    // need to create the function

    std::vector<Type*> args;
    FunctionType* FT = FunctionType::get(
        /*Result=*/Type::getVoidTy(M.getContext()),
        /*Params=*/args,
        /*isVarArg=*/false);

    initFn = Function::Create(FT, Function::ExternalLinkage, name, &M);

    BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", initFn);
    IRBuilder<> irb(BB);
    irb.CreateRetVoid();

    //DEBUG(dbgs() << "created function:" << *BB << "\n");

    addToGlobalCtors(M, initFn);
  }

  return initFn;
}


GlobalVariable* LLTap::InstrumentationPass::addFunctionNameAsStringConstant(Function* calledFn, Module &M) {
  string fname = calledFn->getName();
  size_t size = fname.size() + 1;
  string varname = ".__lltap_fname_";
  varname.append(fname);

  ArrayType* strty = ArrayType::get(IntegerType::get(M.getContext(), 8), size);
  //PointerType* strpty = PointerType::get(strty, 0);

  GlobalVariable* gvar = new GlobalVariable(
      /*Module=*/M,
      /*Type=*/strty,
      /*isConstant=*/true,
      /*Linkage=*/GlobalValue::PrivateLinkage,
      /*Initializer=*/0, // has initializer, specified below
      /*Name=*/varname);
  gvar->setAlignment(1);

  // Constant Definitions
  Constant *data = ConstantDataArray::getString(M.getContext(), fname, true);

  // Global Variable Definitions
  gvar->setInitializer(data);

  return gvar;
}


void LLTap::InstrumentationPass::addCallTarget(Function* calledFn, Module &M) {

  if (calledFn->isIntrinsic()) {
    return;
  }

  string fname = "";
  if (! HookNamespace.empty()) {
    fname += HookNamespace + "_";
  }
  fname += calledFn->getName();

  string varname = "__lltap_fname_";
  varname.append(fname);

  if (M.getNamedValue(varname) == NULL) {

    Function* initfunc = getOrAddInitializerToModule(M);
    IRBuilder<> irb(initfunc->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());

    PointerType* voidptr = PointerType::getUnqual(IntegerType::get(M.getContext(), 8));
    Constant* funcaddr = ConstantExpr::getCast(Instruction::BitCast, calledFn, voidptr);

    Function* callee = M.getFunction(fn_lltap_add_hook);

    Value* val = irb.CreateGlobalStringPtr(fname, varname);

    SmallVector<Value*, 2> args;
    args.push_back(funcaddr);
    args.push_back(val);

    //callee, args,
    irb.CreateCall(callee, args);
  }

}

void LLTap::InstrumentationPass::initializeInstConfig() {

  DEBUG(dbgs() << "Initializing instrumentation configuration\n");

  DEBUG(dbgs() << "going to instrument calls to ");
  for (StringRef s : InstrumentFunctions) {
    instrumentCallsTo.insert(s);
    DEBUG(dbgs() << s << ", ");
  }
  DEBUG(dbgs() << "\n");

  DEBUG(dbgs() << "going to skip instrumentation of calls to ");
  for (StringRef s : NoInstrumentFunctions) {
    noInstrumentCallsTo.insert(s);
    DEBUG(dbgs() << s << ", ");
  }
  DEBUG(dbgs() << "\n");

  if (! InstrumentFunctionsRegexRaw.empty()) {
    instrumentCallsRe = new Regex(InstrumentFunctionsRegexRaw);
    DEBUG(dbgs() << "Function Whitelist Regex: " << InstrumentFunctionsRegexRaw << "\n");
  }

  if (! NoInstrumentFunctionsRegexRaw.empty()) {
    noInstrumentCallsRe = new Regex(NoInstrumentFunctionsRegexRaw);
    DEBUG(dbgs() << "Function Blacklist Regex: " << NoInstrumentFunctionsRegexRaw << "\n");
  }

  instConfigInitialized = true;
}

bool LLTap::InstrumentationPass::shouldBeInstrumented(Function& F) {

  // skip all lltap functions
  if (F.getName().find("lltap") != string::npos) {
    return false;
  }

  if (! instConfigInitialized) {
    initializeInstConfig();
  }

  bool instModeFits = false;

  if (InstMode > InstrumentationMode::inst_ie) {
    if (InstMode == InstrumentationMode::inst_i && (! F.isDeclaration())) {
      instModeFits = true;
    } else if (InstMode == InstrumentationMode::inst_e && (F.isDeclaration())) {
      instModeFits = true;
    }
  } else {
    instModeFits = true;
  }

  bool shouldInstrumentF = false;

  // check if we have whitelist
  if (instrumentCallsTo.size() == 0 && instrumentCallsRe == nullptr) {
    // nothing was specified --> instrument all
    shouldInstrumentF = true;

  } else {
    // check the simple list
    if (instrumentCallsTo.count(F.getName()) > 0) {
      shouldInstrumentF = true;
    }

    // check regex
    if (instrumentCallsRe != nullptr
        && instrumentCallsRe->match(F.getName())) {
      shouldInstrumentF = true;
    }
  }

  // check blacklist
  if (noInstrumentCallsTo.size() > 0) {
    if (noInstrumentCallsTo.count(F.getName())) {
      shouldInstrumentF = false;
    }
  }
  // and blacklist regex
  if (noInstrumentCallsRe != nullptr) {
    if (noInstrumentCallsRe->match(F.getName())) {
      shouldInstrumentF = false;
    }
  }

  return shouldInstrumentF && instModeFits;
}

bool LLTap::InstrumentationPass::runOnFunction(Function &F) {

  bool changed = false;

  //++FunctionsVisited;
  DEBUG(dbgs() << "Function: ");
  DEBUG(dbgs().write_escaped(F.getName()) << '\n');

  if (! shouldBeInstrumented(F)) {
    DEBUG(dbgs() << "...skipping\n");
    return false;
  }

  DEBUG(dbgs() << "got " << F.getNumUses() << " uses\n");
  SmallVector<User*, 16> worklist;
  for (User* user : F.users()) {
    DEBUG(dbgs() << "found user " << *user << "\n");
    worklist.push_back(user);
  }

  for (User* user : worklist) {
    if (isa<CallInst>(user)) {
      CallSite CI(user);
      //CallsFound++;
      changed |= instrumentCall(&CI, *(F.getParent()));
    } else if (! DoNotInstrumentUses) {
      if (F.isVarArg()) {
        errs() << "Warning: cannot replace function pointer to varargs function ("
          << F.getName() << ")\n";
      } else {
        changed |= instrumentUse(user, *F.getParent());
      }
      //UsesFound++;
    }
  }

  return changed;
}

/**
 * Instrument a CallSite in a given Module.
 *
 */
bool LLTap::InstrumentationPass::instrumentCall(CallSite* call, Module& M) {

  bool mod = true;

  Value* calledVal = call->getCalledValue();

  if (! isa<Function>(calledVal)) {
    errs() << "Callsite isn't a call to a known function " << *call->getInstruction() << "\n";
    return false;
  }
  Function* calledFn = call->getCalledFunction();

  if (calledFn->isIntrinsic()) {
    DEBUG(dbgs() << "ignoring intrinsic function " << calledFn->getName() << "\n");
    return false;
  }

  addCallTarget(calledFn, M);

  CallInst* inst = dyn_cast<CallInst>(call->getInstruction());
  Function* hook_fn = getHookFunctionFor(call, M);
  inst->setCalledFunction(hook_fn);

  DEBUG(dbgs() << "hooked call to " << calledFn->getName() << "\n"
      << "with type: " << *calledFn->getFunctionType() << "\n");

  return mod;
}


bool LLTap::InstrumentationPass::instrumentUse(User* user, Module& M) {
  bool changed = false;

  if (isa<StoreInst>(user)) {
    StoreInst* SI = cast<StoreInst>(user);
    Value* val = SI->getValueOperand();
    if (isa<Function>(val)) {
      Function* calledFn = cast<Function>(val);
      addCallTarget(calledFn, M);
      Function* hookFn = getHookFunctionFor(calledFn, M);
      StoreInst* SInew = new StoreInst(hookFn, SI->getPointerOperand(), SI);
      DEBUG(dbgs() << "created new hooked store instruction " << *SInew
          << " replacing " << *SI << "\n");
      SI->eraseFromParent();
      changed = true;
    } else {
      DEBUG(dbgs() << " stored value is not a function in " << *user << "\n");
    }
  }

  return changed;
}


string LLTap::InstrumentationPass::getHookFunctionNameFor(Function* origFunc, CallSite* CS) {
  string hookfnname = string("__lltap_hook_") + string(origFunc->getName());
  if (origFunc->isVarArg()) {
    hookfnname += "_" + mangleFunctionArgs(CS);
  }
  return hookfnname;
}


Function* LLTap::InstrumentationPass::getHookFunctionFor(CallSite* CS, Module& M) {
  Function* origFunc = CS->getCalledFunction();
  string hookfnname = getHookFunctionNameFor(origFunc, CS);
  if (M.getFunction(hookfnname) != NULL) {
    return M.getFunction(hookfnname);
  } else {
    return createHookFunction(StringRef(hookfnname), CS, origFunc, M);
  }
}

Function* LLTap::InstrumentationPass::getHookFunctionFor(Function* origFunc, Module& M) {
  string hookfnname = getHookFunctionNameFor(origFunc);
  if (M.getFunction(hookfnname) != NULL) {
    return M.getFunction(hookfnname);
  } else {
    return createHookFunction(StringRef(hookfnname), origFunc, M);
  }
}


Function* LLTap::InstrumentationPass::createHookFunction(StringRef name, CallSite* call, Function* origFunc, Module& M) {

  std::vector<Type*> ftargs;
  FunctionType* FT = nullptr;
  if (! call->getCalledFunction()->isVarArg()) {
    FT = origFunc->getFunctionType();
  } else {
    for (size_t i = 0; i < call->getNumArgOperands(); ++i) {
      ftargs.push_back(call->getArgument(i)->getType());
    }
    FT = FunctionType::get(
        origFunc->getReturnType(),
        ftargs,
        false);

    ftargs.clear();
  }

  DEBUG(dbgs() << "creating hook function " << name << " with type " << *FT <<
      " numparams " << FT->getNumParams() << "\n");

  // create function
  Function* hookFn = Function::Create(FT, Function::ExternalLinkage, name, &M);
  createHookingCode(origFunc, hookFn, M);

  return hookFn;
}


Function* LLTap::InstrumentationPass::createHookFunction(StringRef name, Function* origFunc, Module& M) {

  std::vector<Type*> ftargs;
  FunctionType* FT = origFunc->getFunctionType();

  DEBUG(dbgs() << "creating hook function " << name << " with type " << *FT <<
      " numparams " << FT->getNumParams() << "\n");

  // create function
  Function* hookFn = Function::Create(FT, Function::ExternalLinkage, name, &M);
  createHookingCode(origFunc, hookFn, M);

  return hookFn;
}




bool LLTap::InstrumentationPass::createHookingCode(Function* origFunc, Function* F, Module& M) {

  FunctionType* origFT = origFunc->getFunctionType();
  FunctionType* FT = F->getFunctionType();
  size_t numparams = FT->getNumParams();
  bool fn_returns_void = FT->getReturnType()->isVoidTy();

  DEBUG(dbgs() << "instrumenting call to function " << origFunc->getName()
      << " with type " << *FT << "\n");

  // append basicblocks for the hook calling
  // --> entry
  // entry --> init
  BasicBlock *entry_BB = BasicBlock::Create(M.getContext(), "entry", F);
  // init --> check_pre (if hooks bitmap != 0)
  //      --> call_orig (if hooks bitmap == 0)
  BasicBlock* init_bb = BasicBlock::Create(M.getContext(), "init", F);

  // check_pre --> call_pre (if hooks bitmap & PRE_HOOK != 0)
  //           --> check_rh
  BasicBlock* check_pre_bb = BasicBlock::Create(M.getContext(), "check_pre", F);
  // call_pre --> check_rh
  BasicBlock* call_pre_bb = BasicBlock::Create(M.getContext(), "call_pre", F);

  // check_rh --> call_rh (if hooks bitmap & REPLACE_HOOK != 0)
  //          --> call_orig
  BasicBlock* check_rh_bb = BasicBlock::Create(M.getContext(), "check_rh", F);
  // call_rh --> check_post
  BasicBlock* call_rh_bb = BasicBlock::Create(M.getContext(), "call_rh", F);
  // call_orig --> return (if hooks bitmap == 0)
  //           --> check_post
  BasicBlock* call_orig_bb = BasicBlock::Create(M.getContext(), "call_orig", F);

  // check_post --> call_post (if hooks bitmap & POST_HOOK != 0)
  //                return
  BasicBlock* check_post_bb = BasicBlock::Create(M.getContext(), "check_post", F);
  // call_post --> return
  BasicBlock* call_post_bb = BasicBlock::Create(M.getContext(), "call_post", F);

  BasicBlock *return_bb = BasicBlock::Create(M.getContext(), "return", F);


  // some "constants" used below
  PointerType* i8ptr = PointerType::getUnqual(IntegerType::get(M.getContext(), 8));
  Constant* orig_func_addr = ConstantExpr::getCast(Instruction::BitCast, origFunc, i8ptr);
  //Value* i8ptr_null = ConstantPointerNull::get(i8ptr);
  Value* i32_zero = ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0);


  // create the instruction in the BBs
  //************************************************************
  // entry and init
  SmallVector<Value*, 4> args;
  Value* ret = nullptr;
  AllocaInst* retval = nullptr;
  AllocaInst** params = new AllocaInst*[numparams];
  Value* hooks_avail = nullptr;
  Value* no_hooks = nullptr;

  {
    IRBuilder<> entry(entry_BB);
    IRBuilder<> init(init_bb);

    size_t i = 0;
    for (auto arg = F->arg_begin(); arg != F->arg_end(); arg++) {
      params[i] = entry.CreateAlloca(arg->getType(), nullptr, "arg");
      init.CreateStore(&*arg, params[i]);
      //DEBUG(dbgs() << "alloca for param " << i << " " << *params[i] << "\n");
      i++;
    }

    if (! fn_returns_void) {
      retval = entry.CreateAlloca(FT->getReturnType(), nullptr, "ret");
    }

    // from entry BB jump to initialization BB
    entry.CreateBr(init_bb);

    // get the hooks availability bitmap
    Function* has_hooks = M.getFunction(fn_lltap_has_hooks);
    args.push_back(orig_func_addr);
    hooks_avail = init.CreateCall(has_hooks, args);
    no_hooks = init.CreateICmpEQ(hooks_avail, i32_zero);
    init.CreateCondBr(no_hooks, call_orig_bb, check_pre_bb);
  }


  //************************************************************
  // pre hook

  Function* get_hook = M.getFunction(fn_lltap_get_hook);
  std::vector<Type*> ftargs;

  {
    IRBuilder<> check_pre(check_pre_bb);
    IRBuilder<> call_pre(call_pre_bb);

    Value* HookType_Enum_pre = ConstantInt::get(IntegerType::getInt32Ty(M.getContext()),
        (uint64_t)HookType::PRE_HOOK);

    for (Type* param : origFT->params()) {
      PointerType* param_ptr = PointerType::getUnqual(param);
      ftargs.push_back(param_ptr);
    }
    //ftargs.push_back(voidptr);
    FunctionType* pre_ft = FunctionType::get(
        Type::getVoidTy(M.getContext()),
        ftargs,
        origFunc->isVarArg());
    PointerType* pre_ptrty = PointerType::getUnqual(pre_ft);

    Value* has_pre_hook = check_pre.CreateICmpNE(
        check_pre.CreateAnd(hooks_avail, HookType_Enum_pre),
        i32_zero);
    check_pre.CreateCondBr(has_pre_hook, call_pre_bb, check_rh_bb);

    args.clear();
    args.push_back(orig_func_addr);
    args.push_back(HookType_Enum_pre);
    Value* preval = call_pre.CreateCall(get_hook, args);

    //DEBUG(dbgs() << "pre hook type = " << *pre_ft << "\n");
    args.clear();
    for (size_t i = 0; i < numparams; ++i) {
      args.push_back(params[i]);
      //DEBUG(dbgs() << "arg " << i <<" type = " << *params[i]->getType()
      //    << " expected " << *FT->getParamType(i) << "\n");
    }
    Value* pre = call_pre.CreateBitCast(preval, pre_ptrty);
    call_pre.CreateCall(pre, args);
    call_pre.CreateBr(check_rh_bb);
  }


  //************************************************************
  // replace hook (rh)

  {
    IRBuilder<> call_rh(call_rh_bb);
    IRBuilder<> check_rh(check_rh_bb);
    IRBuilder<> call_orig(call_orig_bb);

    Value* HookType_Enum_replace = ConstantInt::get(IntegerType::getInt32Ty(M.getContext()),
        (uint64_t)HookType::REPLACE_HOOK);

    // create replace hook
    ftargs.clear();
    for (Type* param : origFT->params()) {
      ftargs.push_back(param);
    }
    // ftargs.push_back(voidptr);

    FunctionType* rh_ft = FunctionType::get(
        FT->getReturnType(),
        ftargs,
        origFunc->isVarArg());
    PointerType* rh_ptr = PointerType::getUnqual(rh_ft);

    Value* has_replace_hook = check_rh.CreateICmpNE(
        check_rh.CreateAnd(hooks_avail, HookType_Enum_replace),
        i32_zero);
    check_rh.CreateCondBr(has_replace_hook, call_rh_bb, call_orig_bb);

    // then call original function
    args.clear();
    for (size_t i = 0; i < numparams; ++i) {
      Value* p = call_orig.CreateLoad(params[i]);
      args.push_back(p);
    }
    ret = call_orig.CreateCall(origFunc, args);
    if (!fn_returns_void) {
      call_orig.CreateStore(ret, retval);
    }

    call_orig.CreateCondBr(no_hooks, return_bb, check_post_bb);

    // else call replace hook function
    args.clear();
    args.push_back(orig_func_addr);
    args.push_back(HookType_Enum_replace);

    Value* rhval = call_rh.CreateCall(get_hook, args);

    //check_rh.CreateStore(rhval, rh);
    args.clear();
    for (size_t i = 0; i < numparams; ++i) {
      Value* p = call_rh.CreateLoad(params[i]);
      args.push_back(p);
    }
    Value* rh = call_rh.CreateBitCast(rhval, rh_ptr);
    ret = call_rh.CreateCall(rh, args);
    if (!fn_returns_void) {
      call_rh.CreateStore(ret, retval);
    }
    call_rh.CreateBr(check_post_bb);
  }


  //************************************************************
  // post hook

  {
    IRBuilder<> check_post(check_post_bb);
    IRBuilder<> call_post(call_post_bb);

    Value* HookType_Enum_post = ConstantInt::get(IntegerType::getInt32Ty(M.getContext()),
        (uint64_t)HookType::POST_HOOK);

    ftargs.clear();
    if (!fn_returns_void) {
      PointerType* ret_ptr = PointerType::getUnqual(FT->getReturnType());
      ftargs.push_back(ret_ptr);
    }
    for (Type* param : origFT->params()) {
      ftargs.push_back(param);
    }
    //ftargs.push_back(voidptr);
    FunctionType* post_ft = FunctionType::get(
        Type::getVoidTy(M.getContext()),
        ftargs,
        origFunc->isVarArg());
    PointerType* post_ptrty = PointerType::getUnqual(post_ft);

    // check for post
    Value* has_post_hook = check_post.CreateICmpNE(
        check_post.CreateAnd(hooks_avail, HookType_Enum_post),
        i32_zero);
    check_post.CreateCondBr(has_post_hook, call_post_bb, return_bb);

    // call post hook
    args.clear();
    args.push_back(orig_func_addr);
    args.push_back(HookType_Enum_post);
    Value* postval = call_post.CreateCall(get_hook, args);

    args.clear();
    if (!fn_returns_void) {
      args.push_back(retval);
    }
    for (size_t i = 0; i < numparams; ++i) {
      Value* p = call_post.CreateLoad(params[i]);
      args.push_back(p);
    }
    Value* post = call_post.CreateBitCast(postval, post_ptrty);
    call_post.CreateCall(post, args);
    call_post.CreateBr(return_bb);
  }

  //************************************************************
  // load retval and return
  {
    IRBuilder<> return_irb(return_bb);

    if (!fn_returns_void) {
      ret = return_irb.CreateLoad(retval);
      return_irb.CreateRet(ret);
    } else {
      return_irb.CreateRetVoid();
    }
  }

  //************************************************************

  //DEBUG(dbgs() << "updated function to contain hooking logic\n" << *F << "\n\n");

  delete[] params;

  return true;
}
