#ifndef RBX_LLVM_HPP
#define RBX_LLVM_HPP
#ifdef ENABLE_LLVM

#include "vmmethod.hpp"
#include "builtin/compiledmethod.hpp"
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>

namespace rubinius {
  class VMLLVMMethod;
  class MethodContext;
  class Task;

  typedef void (*CompiledFunction)(VMLLVMMethod*, Task*, MethodContext* const, int*);

  class VMLLVMMethod : public VMMethod {
  public:
    llvm::Function* function;
    CompiledFunction c_func;

    VMLLVMMethod(STATE, CompiledMethod* meth) :
      VMMethod(state, meth), function(NULL), c_func(NULL) { }
    static void init(const char* path);
    llvm::CallInst* call_operation(Opcode* op, llvm::Value* state,
        llvm::Value*task, llvm::BasicBlock* block);
    llvm::CallInst* call_operation(int index, int width, llvm::Value* task,
        llvm::Value* ctx, llvm::Value* vmm, llvm::BasicBlock* block);
    virtual void compile(STATE);
    virtual void resume(Task* task, MethodContext* ctx);
    llvm::Function* VMLLVMMethod::compile_into_function(const char* name);

    static ExecuteStatus uncompiled_execute(STATE, Task* task, Message& msg);
  };
}

#endif
#endif
