//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include <map>
#include <vector>
#include <iostream>
#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/IR/Use.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/IR/Value.h>
#include <llvm/ADT/iterator_range.h>

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#if LLVM_VERSION_MAJOR >= 4
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#else
#include <llvm/Bitcode/ReaderWriter.h>
#endif
using namespace llvm;
#if LLVM_VERSION_MAJOR >= 4
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
#endif

using namespace std;
/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang will
 * have optnone attribute which would lead to some transform passes disabled, like mem2reg.
 */
#if LLVM_VERSION_MAJOR == 5
struct EnableFunctionOptPass: public FunctionPass {
    static char ID;
    EnableFunctionOptPass():FunctionPass(ID){}
    bool runOnFunction(Function & F) override{
        //cout<<"begin to execute runonfunction in enablefunctionoptpass";
        if(F.hasFnAttribute(Attribute::OptimizeNone))
        {
            F.removeFnAttr(Attribute::OptimizeNone);
        }
        return true;
    }
};

char EnableFunctionOptPass::ID=0;
#endif

	
///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
///Updated 11/10/2017 by fargo: make all functions
///processed by mem2reg before this pass.
struct FuncPtrPass : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  map<int,set<string>> result;
  FuncPtrPass() : ModulePass(ID) {}

  // void showResult(){
  //   //interator map and getvalue
  //   for(){

  //   }
  // }

  void renewResult(int line, string func){
    if(result.count(line)){
      result[line].insert(func);
    }else{
      set<string> s;
      s.insert(func);
      result[line] = s;
    }
  }

  void showResult(){
    map<int,set<string> >::iterator it = result.begin();
    for(it;it!=result.end();it++){
      set<string> s = it->second;
      cout<<it->first<<" : ";
      set<string>::iterator s_it = s.begin();
      if(s_it != s.end()){
        cout<<*s_it;
        s_it++; 
      } 
      for(s_it;s_it!=s.end();s_it++){
        cout<<", "<<*s_it;
      }
      cout<<"\n";
    }
  }

  void dealWithCallInst(int line, CallInst *callinst){
      Function *func = callinst->getCalledFunction();
      for(Function::iterator bi= func->begin(),be = func->end(); bi!=be; bi++){
        BasicBlock &b = *bi;
        for(BasicBlock::iterator ii = b.begin(),ie=b.end();ii!=ie; ii++){
          Instruction *inst = dyn_cast<Instruction>(ii);
          //inst->dump();
          if(isa<ReturnInst>(inst)){
            ReturnInst *instru = dyn_cast<ReturnInst>(inst);
            Value *val = instru->getReturnValue();
            dealWithValue(val,line);
          }
    }
  }
}

    void dealWithFunction(int line, Function *func){
      string name = func->getName();
      renewResult(line,name);
    }

    int getRealArgOffset(unsigned ori_offset){
      unsigned new_offset;

    }

    void dealWithArgument(int line, Argument *argument)
    {
      unsigned offset = argument-> getArgNo();
      Function *parent = argument-> getParent();
      string funcname = parent->getName();
      for(User *U: parent->users()){

        if(CallInst *callinst = dyn_cast<CallInst>(U)){
          Value *arg = callinst->getArgOperand(offset);

          if(callinst!=NULL){
            Function *caller_f = callinst->getCalledFunction();
            string caller_name = caller_f->getName();
            
            if(caller_name != funcname){
  
              for(Function::iterator fb = caller_f->begin(),fe = caller_f->end();fb != fe; fb++){
                BasicBlock &b = *fb;
              for(BasicBlock::iterator bb = b.begin(), be = b.end();bb != be; ++bb){
                Instruction * i= dyn_cast<Instruction>(bb);
                //i->dump();
                if(isa<CallInst>(i)){
                  CallInst *call = dyn_cast<CallInst>(i);

                  //cout<<"this is a callinst"<<endl;
                  //get offset
                  Value *val = call->getCalledValue();
                  //add get_functon_name and compare,
                  if(val!=NULL){
                    //cout<<"getCalledValue"<<endl;
                    //val->dump();
                    int count = 0;
                    for(User::op_iterator arg_begin = call->arg_begin(),arg_end = call->arg_end(); arg_begin != arg_end; ++ arg_begin){
                      //user->value();
                      if(count == offset){
                        Value *arg_val = arg_begin->get();
                        dealWithValue(arg_val,line);
                      }
                    }
                  }
                }
              }
            }
            continue;
            }
          }
          if(isa<PHINode>(arg)){
            PHINode *phinode = dyn_cast<PHINode>(arg);
            dealWithPHINode(line,phinode);
          }else if(isa<Argument>(arg)){
            Argument *argument = dyn_cast<Argument>(arg);
            dealWithArgument(line, argument);
          }else if(isa<Function>(arg)){
            Function *func = dyn_cast<Function>(arg);
            dealWithFunction(line,func);
          }
        }
      }
    }

void dealWithPHINode(int line, PHINode *phinode){
    unsigned num = phinode->getNumIncomingValues();
    for(int i = 0;i < num ;i++ ){
      Value *val = phinode->getIncomingValue(i);
      if(isa<Function>(val)){
        string name = val->getName();
        renewResult(line,name);
      }else if(isa<PHINode>(val)){
        PHINode *phinode = dyn_cast<PHINode>(val);
        dealWithPHINode(line, phinode);
      }else if(isa<Argument>(val)){
        Argument *argument = dyn_cast<Argument>(val);
        dealWithArgument(line, argument);
      }else{
      }
    }
  }



  void dealWithValue(Value *val ,unsigned line){
    if(PHINode *phinode = dyn_cast<PHINode>(val)){
        dealWithPHINode(line,phinode);
    }else if(Argument *argument = dyn_cast<Argument>(val)){
        dealWithArgument(line,argument);
    }else if(CallInst *callinst = dyn_cast<CallInst>(val)){
      dealWithCallInst(line,callinst);
    }
  }


  bool runOnModule(Module &M) override {
    for(Module::iterator fi = M.begin() ,fe=M.end(); fi!=fe; fi++){
      Function &f = *fi;
      for(Function::iterator bi= f.begin(),be = f.end(); bi!=be; bi++){
        BasicBlock &b = *bi;
        for(BasicBlock::iterator ii = b.begin(),ie=b.end();ii!=ie; ii++){
          Instruction *inst = dyn_cast<Instruction>(ii);
          if(isa<CallInst>(ii)){
            CallInst *call = dyn_cast<CallInst>(inst);
            if(call!=NULL){
              //code for test00.c
              Function *f = call->getCalledFunction();
              if(f){
                if(f->isIntrinsic()){
                  continue;
                }

                unsigned line = call->getDebugLoc().getLine();
                string name = f->getName();
                renewResult(line,name);

              }else{
                Value *val = call->getCalledValue();
                unsigned line = call->getDebugLoc().getLine();
                dealWithValue(val,line);
              }
            }
          }
          }
        }
      }
      showResult();
      return false;
    }
};


char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass", "Print function call instruction");

static cl::opt<std::string>
InputFilename(cl::Positional,
              cl::desc("<filename>.bc"),
              cl::init(""));


int main(int argc, char **argv) {
   LLVMContext &Context = getGlobalContext();
   SMDiagnostic Err;
   // Parse the command line to read the Inputfilename
   cl::ParseCommandLineOptions(argc, argv,
                              "FuncPtrPass \n My first LLVM too which does not do much.\n");


   // Load the input module
   std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
   if (!M) {
      Err.print(argv[0], errs());
      return 1;
   }

   llvm::legacy::PassManager Passes;
    
   ///Remove functions' optnone attribute in LLVM5.0
   #if LLVM_VERSION_MAJOR == 5
   Passes.add(new EnableFunctionOptPass());
   #endif
   ///Transform it to SSA
   Passes.add(llvm::createPromoteMemoryToRegisterPass());

   /// Your pass to print Function and Call Instructions
   Passes.add(new FuncPtrPass());
   //Passes.run(*M.get());
   
   bool updated = Passes.run (*M.get ());

 
 // if(updated)
 // {
 //      //errs()<<"changed\n";
 //      std::error_code EC;
 //      std::unique_ptr<ToolOutputFile> Out(new ToolOutputFile(InputFilename, EC, sys::fs::F_None));
 //      WriteBitcodeToFile(M.get(), Out->os());
 //      Out->keep();
 // }
}

