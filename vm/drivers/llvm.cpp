#include <iostream>
#include <fstream>

#include <sys/stat.h>
#include "vm/llvm.hpp"

#include "prelude.hpp"
#include "vm/environment.hpp"
#include "vm/oop.hpp"
#include "vm/type_info.hpp"
#include "vm/exception.hpp"
#include "vm/llvm.hpp"
#include "vm/vm.hpp"

#include "builtin/task.hpp"
#include "vm/compiled_file.hpp"
#include "vm/object_utils.hpp"

#include "sha1.h"

#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Linker.h>

using namespace std;
using namespace rubinius;

static llvm::Function* output_method(Environment& env, CompiledMethod* method,
    std::vector<llvm::GlobalValue*> &gvs) {
  SHA1 sha;
  rubinius::VMLLVMMethod llvm(env.state, method);
  for(size_t i = 0; i < llvm.total; i++) {
    sha << (unsigned int)llvm.opcodes[i];
  }

  sha << (unsigned int)llvm.total_args;
  sha << (unsigned int)llvm.required_args;
  sha << (unsigned int)llvm.splat_position;

  unsigned digest[5];
  if(!sha.Result(digest)) {
    cout << "Corrupt SHA1\n";
    exit(1);
  }

  stringstream ss;
  ss << "_XS";
  ss.setf(ios::hex,ios::basefield);

  for(int i = 0; i < 5 ; i++) {
    ss << digest[i];
  }

  cout << ss.str() << endl;
  llvm::Function* func = llvm.compile_into_function(ss.str().c_str());
  gvs.push_back(func);

  Tuple* literals = method->literals();
  for(size_t i = 0; i < literals->num_fields(); i++) {
    if(CompiledMethod* sub = try_as<CompiledMethod>(literals->at(state, i))) {
      output_method(env, sub, gvs);
    }
  }

  return func;
}

int main(int argc, char** argv) {
  Environment env;

  if(argc != 3) {
    cout << "Usage: " << argv[0] << " <dir/file.rbc> <out.bc>\n";
    return 1;
  }

  std::vector<llvm::GlobalValue*> gvs;

  struct stat st;

  if(stat(argv[1], &st) == -1) {
    cout << "Unable to handle '" << argv[1] << "'. Does not exist.\n";
    return 1;
  }

  try {
    if(st.st_mode & S_IFDIR) {

      DIR* dir = opendir(argv[1]);
      struct dirent* dent;

      while((dent = readdir(dir) != NULL)) {
        std::ifstream stream(dent->d_name);
        if(!stream) {
          cout << "Unable to open '" << dent->d_name << "'\n";
          return 1;
        }

        cout << dent->d_name << "\n";

        CompiledFile* cf = CompiledFile::load(stream);
        if(cf->magic != "!RBIX") {
          cout << argv[1] << " not a valid .rbc file.\n";
          return 1;
        }

        CompiledMethod* script = as<CompiledMethod>(cf->body(env.state));
        output_method(env, script, gvs);

        delete cf;
      }

      closedir(dir);
    } else {
      std::ifstream stream(argv[1]);
      if(!stream) {
        cout << "Unable to open '" << argv[1] << "'\n";
        return 1;
      }

      cout << dent->d_name << "\n";

      CompiledFile* cf = CompiledFile::load(stream);
      if(cf->magic != "!RBIX") {
        cout << argv[1] << " not a valid .rbc file.\n";
        return 1;
      }

      CompiledMethod* script = as<CompiledMethod>(cf->body(env.state));
      output_method(env, script, gvs);

      delete cf;
    }

    llvm::Module* mod = rubinius::VM::llvm_module();
    llvm::PassManager passes;
    passes.add(new llvm::TargetData(mod));

    passes.add(llvm::createGVExtractionPass(gvs, false, false));
    passes.add(llvm::createGlobalDCEPass());
    passes.add(llvm::createDeadTypeEliminationPass());
    passes.add(llvm::createStripDeadPrototypesPass());

    if(stat(argv[2], &st) == -1) {
      std::ofstream output(argv[2]);
      passes.add(llvm::CreateBitcodeWriterPass(output));
      passes.run(*mod);
    } else {
      std::string error;
      llvm::Module* existing;
      if(llvm::MemoryBuffer* buffer = llvm::MemoryBuffer::getFile(argv[2], &error)) {
        existing = llvm::ParseBitcodeFile(buffer, &error);
        delete buffer;
      } else {
        cout << "unable to open existing .bc\n";
        return 1;
      }

      llvm::Linker linker("rubinius", existing);
      passes.run(*mod);
      linker.LinkInModule(mod);

      std::ofstream output(argv[2]);
      llvm::WriteBitcodeToFile(existing, output);
    }

    return 0;
  } catch(Assertion &e) {
    std::cout << "VM Assertion:" << std::endl;
    std::cout << "  " << e.reason << std::endl;
    e.print_backtrace();

    std::cout << "Ruby backtrace:" << std::endl;
    env.state->print_backtrace();
  } catch(RubyException &e) {
    // Prints Ruby backtrace, and VM backtrace if captured
    e.show(env.state);
  } catch(TypeError &e) {
    std::cout << "Type Error detected:" << std::endl;
    TypeInfo* wanted = env.state->find_type(e.type);

    if(!e.object->reference_p()) {
      std::cout << "  Tried to use non-reference value " << e.object;
    } else {
      TypeInfo* was = env.state->find_type(e.object->obj_type);
      std::cout << "  Tried to use object of type " <<
        was->type_name << " (" << was->type << ")";
    }

    std::cout << " as type " << wanted->type_name << " (" <<
      wanted->type << ")" << std::endl;

    e.print_backtrace();

    std::cout << "Ruby backtrace:" << std::endl;
    env.state->print_backtrace();
  } catch(std::runtime_error& e) {
    std::cout << "Runtime exception: " << e.what() << std::endl;
  } catch(VMException &e) {
    std::cout << "Unknown VM exception detected." << std::endl;
    e.print_backtrace();
  } catch(std::string e) {
    std::cout << e << std::endl;
  } catch(...) {
    std::cout << "Unknown exception detected." << std::endl;
  }
}
