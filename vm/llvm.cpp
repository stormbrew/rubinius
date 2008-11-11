#ifdef ENABLE_LLVM

#include "llvm.hpp"

#include "message.hpp"

#include "vm/object_utils.hpp"

#include "builtin/compiledmethod.hpp"
#include "builtin/contexts.hpp"
#include "builtin/iseq.hpp"
#include "builtin/task.hpp"
#include "builtin/tuple.hpp"
#include "builtin/symbol.hpp"
#include "builtin/string.hpp"

#include <llvm/Module.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Constants.h>
#include <llvm/GlobalVariable.h>
#include <llvm/Function.h>
#include <llvm/CallingConv.h>
#include <llvm/BasicBlock.h>
#include <llvm/Instructions.h>
#include <llvm/InlineAsm.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/TypeSymbolTable.h>
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/ModuleProvider.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include <llvm/Support/CommandLine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Analysis/Verifier.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>

#define INLINE_THRESHOLD 1000

namespace rubinius {
  using namespace llvm;

  static llvm::Module* operations = NULL;
  static llvm::ExistingModuleProvider* mp = NULL;
  static llvm::ExecutionEngine* engine = NULL;

  // static llvm::Function* puts = NULL;

  llvm::Module* VM::llvm_module() {
    return operations;
  }

  /* Clean up the memory in the statics.
   * TODO remove the statics entirely. */
  void VM::llvm_cleanup() {
    /* We can't also delete mp and operations, since deleting engine
     * cascade deletes mp and operations internally. */
    delete engine;
    operations = NULL;
    mp = NULL;
    engine = NULL;
  }

  void VMLLVMMethod::init(const char* path) {
    llvm::ExceptionHandling = true;
    llvm::UnwindTablesMandatory = true;

    std::string error;
    if(!operations) {
      if(MemoryBuffer* buffer = MemoryBuffer::getFile(path, &error)) {
        operations = ParseBitcodeFile(buffer, &error);
        delete buffer;
      } else {
        operations = NULL;
        throw std::runtime_error(std::string("Unable to open LLVM operations file") + error);
      }

      mp = new ExistingModuleProvider(operations);
      engine = ExecutionEngine::create(mp);
    }
#if 0

    llvm::PointerType* PointerTy_5 = llvm::PointerType::get(llvm::IntegerType::get(8), 0);

    std::vector<const Type*>FuncTy_8_args;
    FuncTy_8_args.push_back(PointerTy_5);
    FunctionType* FuncTy_8 = FunctionType::get(
        /*Result=*/ llvm::IntegerType::get(32),
        /*Params=*/ FuncTy_8_args,
        /*isVarArg=*/ false);

    puts = Function::Create(
        /*Type=*/ FuncTy_8,
        /*Linkage=*/ GlobalValue::ExternalLinkage,
        /*Name=*/ "puts", operations); // (external, no body)
#endif

  }

  static CallInst* call_function(std::string name, Value* task,
                                 Value* ctx, Value* vmm, BasicBlock* block) {
    Function* func = operations->getFunction(name);
    if(!func) {
      std::string str = std::string("Unable to find: ");
      str += name;

      throw std::runtime_error(str);
    }

    std::vector<Value*> args(0);
    args.push_back(vmm);
    args.push_back(task);
    args.push_back(ctx);

    return CallInst::Create(func, args.begin(), args.end(), "", block);
  }

  CallInst* VMLLVMMethod::call_operation(int index, int width, Value* task,
                                         Value* ctx, Value* vmm, BasicBlock* block) {
    const char* name = InstructionSequence::get_instruction_name(opcodes[index]);
    Function* func = operations->getFunction(std::string(name));
    if(!func) {
      std::string str = std::string("Unable to find: ");
      str += name;

      throw std::runtime_error(str);
    }

    std::vector<Value*> args(0);
    args.push_back(vmm);
    args.push_back(task);
    args.push_back(ctx);

    switch(width - 1) {
    case 2:
      args.push_back(ConstantInt::get(Type::Int32Ty, opcodes[index + 1]));
      args.push_back(ConstantInt::get(Type::Int32Ty, opcodes[index + 2]));
      break;
    case 1:
      args.push_back(ConstantInt::get(Type::Int32Ty, opcodes[index + 1]));
      break;
    }

    return CallInst::Create(func, args.begin(), args.end(), "", block);
  }

  CallInst* VMLLVMMethod::call_operation(Opcode* op, Value* task,
                                         Value* js, BasicBlock* block) {
    const char* name = InstructionSequence::get_instruction_name(op->op);
    Function* func = operations->getFunction(std::string(name));
    if(!func) {
      std::string str = std::string("Unable to find: ");
      str += name;

      throw std::runtime_error(str);
    }

    std::vector<Value*> args(0);
    args.push_back(task);
    args.push_back(js);

    switch(op->args) {
    case 2:
      args.push_back(ConstantInt::get(Type::Int32Ty, op->arg1));
      args.push_back(ConstantInt::get(Type::Int32Ty, op->arg2));
      break;
    case 1:
      args.push_back(ConstantInt::get(Type::Int32Ty, op->arg1));
      break;
    }

    return CallInst::Create(func, args.begin(), args.end(), "", block);
  }

#if 0
  void show(const char* data, BasicBlock* block) {
    llvm::ArrayType* ArrayTy_0 = llvm::ArrayType::get(llvm::IntegerType::get(8), strlen(data) + 1);
    GlobalVariable* glob = new GlobalVariable(ArrayTy_0,
        true, GlobalValue::InternalLinkage, 0, data, operations);
    Constant* str = ConstantArray::get(data, true);

    std::vector<Constant*> const_ptr_7_indices;
    Constant* const_int32_8 = Constant::getNullValue(llvm::IntegerType::get(32));
    const_ptr_7_indices.push_back(const_int32_8);
    const_ptr_7_indices.push_back(const_int32_8);
    Constant* const_ptr_7 = ConstantExpr::getGetElementPtr(glob, &const_ptr_7_indices[0], const_ptr_7_indices.size() );

    glob->setInitializer(str);

    std::vector<Value*> args(0);
    args.push_back(const_ptr_7);

    CallInst::Create(puts, args.begin(), args.end(), "tmp", block);
  }
#endif

#if 0
  /* Copied originally from opt */
  static void add_passes(PassManager& PM) {
    PM.add(createVerifierPass());             // Verify that input is correct

    PM.add(createRaiseAllocationsPass());     // call %malloc -> malloc inst
    PM.add(createCFGSimplificationPass());    // Clean up disgusting code
    PM.add(createPromoteMemoryToRegisterPass());// Kill useless allocas
    PM.add(createGlobalOptimizerPass());      // Optimize out global vars
    PM.add(createGlobalDCEPass());            // Remove unused fns and globs
    PM.add(createIPConstantPropagationPass());// IP Constant Propagation
    PM.add(createDeadArgEliminationPass());   // Dead argument elimination
    PM.add(createInstructionCombiningPass()); // Clean up after IPCP & DAE
    PM.add(createCFGSimplificationPass());    // Clean up after IPCP & DAE

    PM.add(createPruneEHPass());              // Remove dead EH info

    PM.add(createFunctionInliningPass(INLINE_THRESHOLD));   // Inline small functions
    PM.add(createArgumentPromotionPass());    // Scalarize uninlined fn args

    PM.add(createTailDuplicationPass());      // Simplify cfg by copying code
    PM.add(createSimplifyLibCallsPass());     // Library Call Optimizations
    PM.add(createInstructionCombiningPass()); // Cleanup for scalarrepl.
    PM.add(createJumpThreadingPass());        // Thread jumps.
    PM.add(createCFGSimplificationPass());    // Merge & remove BBs
    PM.add(createScalarReplAggregatesPass()); // Break up aggregate allocas
    PM.add(createInstructionCombiningPass()); // Combine silly seq's
    PM.add(createCondPropagationPass());      // Propagate conditionals

    PM.add(createTailCallEliminationPass());  // Eliminate tail calls
    PM.add(createCFGSimplificationPass());    // Merge & remove BBs
    PM.add(createReassociatePass());          // Reassociate expressions
    PM.add(createLoopRotatePass());
    PM.add(createLICMPass());                 // Hoist loop invariants
    PM.add(createLoopUnswitchPass());         // Unswitch loops.
    PM.add(createLoopIndexSplitPass());       // Index split loops.
    // FIXME : Removing instcombine causes nestedloop regression.
    PM.add(createInstructionCombiningPass());
    PM.add(createIndVarSimplifyPass());       // Canonicalize indvars
    PM.add(createLoopDeletionPass());         // Delete dead loops
    PM.add(createLoopUnrollPass());           // Unroll small loops
    PM.add(createInstructionCombiningPass()); // Clean up after the unroller
    PM.add(createGVNPass());                  // Remove redundancies
    PM.add(createMemCpyOptPass());            // Remove memcpy / form memset
    PM.add(createSCCPPass());                 // Constant prop with SCCP

    // Run instcombine after redundancy elimination to exploit opportunities
    // opened up by them.
    PM.add(createInstructionCombiningPass());
    PM.add(createCondPropagationPass());      // Propagate conditionals

    PM.add(createDeadStoreEliminationPass()); // Delete dead stores
    PM.add(createAggressiveDCEPass());        // Delete dead instructions
    PM.add(createCFGSimplificationPass());    // Merge & remove BBs
    PM.add(createStripDeadPrototypesPass());  // Get rid of dead prototypes
    PM.add(createConstantMergePass());        // Merge dup global constants

  }
#endif

#if 0
  static Function* create_function(const char* name) {
    std::stringstream stream;
    stream << "_XJIT_" << strlen(name) << name << "_" << operations->size();

    const Type* task_type  = operations->getTypeByName(std::string("struct.rubinius::Task"));
    std::string js("struct.jit_state");
    const Type* obj_type = operations->getTypeByName(js);

    Type* stack_type = PointerType::get(obj_type, 0);

    Function* func = cast<Function>(
        operations->getOrInsertFunction(stream.str(), Type::VoidTy,
          PointerType::getUnqual(task_type), stack_type,
          PointerType::getUnqual(Type::Int32Ty), (void *)NULL));

    return func;
  }

  static BasicBlock** construct_blocks(Function* func,
      std::vector<Opcode*> ops, Value* next_pos) {

    BasicBlock* top = BasicBlock::Create("top", func);
    size_t num_blocks = ops[ops.size() - 1]->block + 1;

    /*   if we have to bail, set next_pos to -1, so the caller knows. */
    BasicBlock* def = BasicBlock::Create("error", func);
    new StoreInst(ConstantInt::get(Type::Int32Ty, (uint64_t)-1), next_pos, def);
    ReturnInst::Create(def);

    /*   load in the current value of next_pos, and branch to it. */
    Value* pos = new LoadInst(next_pos, "get_pos", top);
    SwitchInst* sw = SwitchInst::Create(pos, def, num_blocks, top);

    /* Create all the blocks and initialize them in the switch */
    BasicBlock** blocks = (BasicBlock**)calloc(num_blocks, sizeof(BasicBlock*));
    for(size_t i = 0; i < num_blocks; i++) {
      std::stringstream stream;
      stream << "block" << i;
      blocks[i] = BasicBlock::Create(stream.str(), func);
      sw->addCase(ConstantInt::get(Type::Int32Ty, i), blocks[i]);
    }

    return blocks;
  }
#endif
  static bool is_terminator(opcode op) {
    switch(op) {
    case InstructionSequence::insn_send_method:
    case InstructionSequence::insn_send_stack:
    case InstructionSequence::insn_send_stack_with_block:
    case InstructionSequence::insn_send_stack_with_splat:
    case InstructionSequence::insn_send_super_stack_with_splat:
    case InstructionSequence::insn_send_super_stack_with_block:
    case InstructionSequence::insn_meta_send_op_plus:
    case InstructionSequence::insn_meta_send_op_minus:
    case InstructionSequence::insn_meta_send_op_equal:
    case InstructionSequence::insn_meta_send_op_lt:
    case InstructionSequence::insn_meta_send_op_gt:
    case InstructionSequence::insn_meta_send_op_tequal:
    case InstructionSequence::insn_meta_send_op_nequal:
    case InstructionSequence::insn_meta_send_call:
      return true;
    }

    return false;
  }

  static bool is_send(opcode op) {
    switch(op) {
    case InstructionSequence::insn_send_method:
    case InstructionSequence::insn_send_stack:
    case InstructionSequence::insn_send_stack_with_block:
    case InstructionSequence::insn_send_stack_with_splat:
    case InstructionSequence::insn_send_super_stack_with_splat:
    case InstructionSequence::insn_send_super_stack_with_block:
    case InstructionSequence::insn_meta_send_op_plus:
    case InstructionSequence::insn_meta_send_op_minus:
    case InstructionSequence::insn_meta_send_op_equal:
    case InstructionSequence::insn_meta_send_op_lt:
    case InstructionSequence::insn_meta_send_op_gt:
    case InstructionSequence::insn_meta_send_op_tequal:
    case InstructionSequence::insn_meta_send_op_nequal:
    case InstructionSequence::insn_meta_send_call:
      return true;
    }

    return false;
  }

  static BasicBlock* create_block(BasicBlock** blocks, int index, Function* func) {
    if(blocks[index]) {
      return blocks[index];
    }
    std::stringstream stream;
    stream << "insn" << index;
    BasicBlock* block = BasicBlock::Create(stream.str().c_str(), func);
    blocks[index] = block;
    return block;
  }

  void VMLLVMMethod::compile(STATE) {
    std::stringstream stream;
    Symbol* sym = original->name();
    const char* name;
    if(sym->nil_p()) {
      name = "NONAME";
    } else {
      name = original->name()->c_str(state);
    }
    stream << "_XJIT_" << strlen(name) << name << "_" << operations->size();

    function = compile_into_function(stream.str().c_str());
    c_func = (CompiledFunction)engine->getPointerToFunction(function);
  }

  llvm::Function* VMLLVMMethod::compile_into_function(const char* name) {
    const Type* vmm_type  = operations->getTypeByName("struct.rubinius::VMMethod");
    const Type* task_type = operations->getTypeByName("struct.rubinius::Task");
    const Type* ctx_type  = operations->getTypeByName("struct.rubinius::MethodContext");

    Function* func = cast<Function>(
        operations->getOrInsertFunction(name, Type::VoidTy,
          PointerType::getUnqual(vmm_type),
          PointerType::getUnqual(task_type),
          PointerType::getUnqual(ctx_type),
          PointerType::getUnqual(Type::Int32Ty),
          (void *)NULL));

    Function::arg_iterator args = func->arg_begin();
    Value* vmm = args++;
    vmm->setName("vmm");

    Value* task = args++;
    task->setName("task");

    Value* ctx = args++;
    ctx->setName("ctx");

    Value* pos = args++;
    pos->setName("pos");

    BasicBlock* top = BasicBlock::Create("top", func);

    /* We collect up all the calls to the operation functions so we can
     * inline them later. */
    std::vector<CallInst*> calls(0);

    BasicBlock** blocks = new BasicBlock*[total];
    for(size_t i = 0; i < total; i++) {
      blocks[i] = NULL;
    }

    for(size_t idx = 0; idx < total;) {
      size_t width = InstructionSequence::instruction_width(opcodes[idx]);

      // The current block
      BasicBlock* block = create_block(blocks, idx, func);

      // Where we go next
      BasicBlock* next;
      if(idx + width >= total) {
        next = NULL;
      } else {
        next = create_block(blocks, idx + width, func);
      }

      CallInst* call = NULL;

      // Set our virtual ip to point to beyond the current instruction
      new StoreInst(ConstantInt::get(Type::Int32Ty, idx + width), pos, block);

      switch(opcodes[idx]) {
      case InstructionSequence::insn_goto:
        BasicBlock* dest = create_block(blocks, opcodes[idx + 1], func);
        BranchInst::Create(dest, block);
        break;
      case InstructionSequence::insn_goto_if_true:
      case InstructionSequence::insn_goto_if_false:
      case InstructionSequence::insn_goto_if_defined: {
        BasicBlock* dest = create_block(blocks, opcodes[idx + 1], func);

        std::string func_name;
        switch(opcodes[idx]) {
        case InstructionSequence::insn_goto_if_true:
          func_name = std::string("jit_goto_if_true");
          break;
        case InstructionSequence::insn_goto_if_false:
          func_name = std::string("jit_goto_if_false");
          break;
        case InstructionSequence::insn_goto_if_defined:
          func_name = std::string("jit_goto_if_defined");
          break;
        }

        call = call_function(func_name, task, ctx, vmm, block);
        call->setName("goto");
        ICmpInst* cmp = new ICmpInst(ICmpInst::ICMP_EQ, call,
            ConstantInt::get(Type::Int8Ty, cExecuteRestart), "cmp", block);

        BranchInst::Create(dest, next, cmp, block);
        break;
      }
      case InstructionSequence::insn_halt:
        new StoreInst(ConstantInt::get(Type::Int32Ty, (uint64_t)-1), pos, block);
        ReturnInst::Create(block);
        break;
      default:
        call = call_operation(idx, width, task, ctx, vmm, block);
        if(is_send(opcodes[idx])) {
          // Set up a comparison for the upcoming return value from the send
          ICmpInst* cmp = new ICmpInst(ICmpInst::ICMP_EQ, call,
              ConstantInt::get(Type::Int8Ty, cExecuteRestart), "cmp", block);
          // If the send returns true, then we will reset the execution state.
          // Specifically, it will cause VMMethod::resume to return
          BasicBlock* send = BasicBlock::Create("send", func);

          // Perform the comparison and go to next or send as above
          // 'block' is where the branch instruction is stored
          BranchInst::Create(send, next, cmp, block);

          // Add 'store' and 'return' instructions to the 'send' basic block
          new StoreInst(ConstantInt::get(Type::Int32Ty, idx + width), pos, send);
          ReturnInst::Create(send);

          // Make this flow look sane in the LLVM debugging output
          send->moveAfter(block);
        } else if(is_terminator(opcodes[idx])) {
          /* Setup where to return to when we're re-entered. */
          new StoreInst(ConstantInt::get(Type::Int32Ty, idx + width), pos, block);
          ReturnInst::Create(block);
        } else if(opcodes[idx] == InstructionSequence::insn_ret ) {
          /* -2 signals that we're done for reals now. */
          new StoreInst(ConstantInt::get(Type::Int32Ty, (uint64_t)-2), pos, block);
          ReturnInst::Create(block);
        } else {
          BranchInst::Create(next, block);
        }
      }

      if(call) calls.push_back(call);
      idx += width;
    }

    // Create the switch at the top
    /*   if we have to bail, set next_pos to -1, so the caller knows. */
    BasicBlock* def = BasicBlock::Create("error", func);
    new StoreInst(ConstantInt::get(Type::Int32Ty, (uint64_t)-1), pos, def);
    ReturnInst::Create(def);

    size_t use_blocks = 0;
    for(size_t i = 0; i < total; i++) {
      if(blocks[i]) use_blocks++;
    }

    /*   load in the current value of next_pos, and branch to it. */
    Value* current_pos = new LoadInst(pos, "get_pos", top);
    SwitchInst* sw = SwitchInst::Create(current_pos, def, use_blocks, top);

    for(size_t i = 0; i < total; i++) {
      if(blocks[i]) sw->addCase(ConstantInt::get(Type::Int32Ty, i), blocks[i]);
    }

    delete[] blocks;

    /* Now, manually inline all those calls to operation functions. */
    for(std::vector<CallInst*>::iterator i = calls.begin(); i != calls.end(); i++) {
      CallInst* ci = *i;
      llvm::InlineFunction(ci);
    }

    FunctionPassManager fpm(new ExistingModuleProvider(func->getParent()));

    fpm.add(new TargetData(func->getParent()));
    fpm.add(createInstructionCombiningPass());
    fpm.add(createPromoteMemoryToRegisterPass());
    fpm.add(createConstantPropagationPass());
    fpm.add(createScalarReplAggregatesPass());
    fpm.add(createAggressiveDCEPass());
    fpm.add(createVerifierPass());
    fpm.add(createCFGSimplificationPass());
    fpm.add(createDeadStoreEliminationPass());
    fpm.add(createInstructionCombiningPass());
    fpm.run(*func);

    // std::cout << "Function: " << name << "\n";
    // std::cout << *func << "\n";

    return func;
  }

#if 0
  void VMLLVMMethod::old_compile(STATE) {
    const char* name = original->name()->c_str(state);
    Function* func = create_function(name);

    Function::arg_iterator args = func->arg_begin();
    Value* task = args++;
    task->setName("task");

    Value* js = args++;
    js->setName("js");

    Value* next_pos = args++;
    next_pos->setName("next_pos");

    std::vector<Opcode*> ops = create_opcodes();
    BasicBlock** blocks = construct_blocks(func, ops, next_pos);

    BasicBlock* last = NULL;
    BasicBlock* cur  = NULL;
    int cur_block = -1;

    /* We collect up all the calls to the operation functions so we can
     * inline them later. */
    std::vector<CallInst*> calls(0);

    for(std::vector<Opcode*>::iterator i = ops.begin(); i != ops.end(); i++) {
      Opcode* op = *i;
      /* Having this be conditional lets us change +cur+ for the rest of a block.
       * This is used to implement branch's 2nd position without allocating
       * continuation style slots. */
      if(cur_block < 0 || (int)op->block != cur_block) {
        cur = blocks[op->block];
      }

      cur_block = op->block;

      if(op->start_block) {
        /* Setup a branch from the last block to this one if it doesn't have a
         * terminator on the end. */
        if(!isa<TerminatorInst>(last->back())) {
          BranchInst::Create(cur, last);
        }
      }

      last = cur;

      // show(InstructionSequence::get_instruction_name(op->op), cur);

      CallInst* call = NULL;

      switch(op->op) {
      case InstructionSequence::insn_goto:
        BranchInst::Create(blocks[ops[op->arg1]->block], cur);
        break;
      case InstructionSequence::insn_goto_if_true:
      case InstructionSequence::insn_goto_if_false:
      case InstructionSequence::insn_goto_if_defined: {
        /* Remove the argument from being used, since we used it directly
         * here. */
        op->args--;
        std::string func_name;
        switch(op->op) {
        case InstructionSequence::insn_goto_if_true:
          func_name = std::string("jit_goto_if_true");
          break;
        case InstructionSequence::insn_goto_if_false:
          func_name = std::string("jit_goto_if_false");
          break;
        case InstructionSequence::insn_goto_if_defined:
          func_name = std::string("jit_goto_if_defined");
          break;
        }

        call = call_function(func_name, task, js, cur);
        call->setName("goto");
        ICmpInst* cmp = new ICmpInst(ICmpInst::ICMP_EQ, call,
            ConstantInt::get(Type::Int8Ty, 1), "cmp", cur);
        BasicBlock* bb = BasicBlock::Create("span", func);
        bb->moveAfter(cur);
        BranchInst::Create(blocks[ops[op->arg1]->block], bb, cmp, cur);
        cur = bb;
        break;
      }
      case InstructionSequence::insn_halt:
        new StoreInst(ConstantInt::get(Type::Int32Ty, (uint64_t)-1), next_pos, cur);
        break;
      default:
        call = call_operation(op, task, js, cur);
      }

      if(op->is_send()) {
        assert(call);
        call->setName("maybe_send");
        // Set up a comparison for the upcoming return value from the send
        ICmpInst* cmp = new ICmpInst(ICmpInst::ICMP_EQ, call,
            ConstantInt::get(Type::Int8Ty, 1), "cmp", cur);
        // If the send returns true, then we will reset the execution state.
        // Specifically, it will cause VMMethod::resume to return
        BasicBlock* send = BasicBlock::Create("send", func);
        // If the send returns false, then we will 'stay'
        BasicBlock* stay = blocks[cur_block + 1];

        // Perform the comparison and go to stay or send as above
        // 'cur' is where the branch instruction is stored
        BranchInst::Create(send, stay, cmp, cur);

        // Add 'store' and 'return' instructions to the 'send' basic block
        new StoreInst(ConstantInt::get(Type::Int32Ty, op->block + 1), next_pos, send);
        ReturnInst::Create(send);

        // Make this flow look sane in the LLVM debugging output
        send->moveAfter(cur);
        // The current block is now the one after 'send'
        cur = stay;

      } else if(op->op == InstructionSequence::insn_ret) {
        /* -2 signals that we're done for reals now. */
        new StoreInst(ConstantInt::get(Type::Int32Ty, (uint64_t)-2), next_pos, cur);
        ReturnInst::Create(cur);
      } else if(op->is_terminator()) {
        /* Setup where to return to when we're re-entered. */
        new StoreInst(ConstantInt::get(Type::Int32Ty, op->block + 1), next_pos, cur);
        ReturnInst::Create(cur);
      }

      /* Now, inline it! We don't hinge on the optimizer to do this anymore. */
      if(call) {
        calls.push_back(call);
      }
    }

    /* stick a return on the end if there isn't already one. */
    if(!isa<ReturnInst>(cur->back())) {
      ReturnInst::Create(cur);
    }

    /* Now, manually inline all those calls to operation functions. */

    for(std::vector<CallInst*>::iterator i = calls.begin(); i != calls.end(); i++) {
      CallInst* ci = *i;
      llvm::InlineFunction(ci);
    }

    FunctionPassManager fpm(new ExistingModuleProvider(func->getParent()));

    fpm.add(new TargetData(func->getParent()));
    fpm.add(createInstructionCombiningPass());
    fpm.add(createPromoteMemoryToRegisterPass());
    fpm.add(createConstantPropagationPass());
    fpm.add(createScalarReplAggregatesPass());
    fpm.add(createAggressiveDCEPass());
    fpm.add(createVerifierPass());
    fpm.add(createCFGSimplificationPass());
    fpm.add(createDeadStoreEliminationPass());
    fpm.add(createInstructionCombiningPass());
    fpm.run(*func);

    function = func;

    c_func = (CompiledFunction)engine->getPointerToFunction(func);
  }
#endif

  ExecuteStatus VMLLVMMethod::uncompiled_execute(STATE,
                                        Task* task, Message& msg) {
    CompiledMethod* cm = as<CompiledMethod>(msg.method);

    VMLLVMMethod* real = new VMLLVMMethod(state, cm);
    cm->backend_method_ = real;

    real->compile(state);
    return VMMethod::execute(state, task, msg);
  }

  void VMLLVMMethod::resume(Task* task, MethodContext* ctx) {
    // std::cout << "Enter: " << (void*)ctx << "\n";
    c_func(this, task, ctx, &ctx->ip);
    // std::cout << "Exit:  " << (void*)task->active();
    // std::cout << " ip = " << ctx->ip << "\n";
    if(ctx->ip == -1) {
      throw Task::Halt("Task halted");
    }
  }
}

#endif
