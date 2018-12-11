//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include<stdio.h>
#include<iostream>

using namespace clang;
using namespace std;

#include "Environment.h"

class InterpreterVisitor : 
   public EvaluatedExprVisitor<InterpreterVisitor> {
public:
   explicit InterpreterVisitor(const ASTContext &context, Environment * env)
   : EvaluatedExprVisitor(context), mEnv(env) {
   }
   virtual ~InterpreterVisitor() {}

   virtual void VisitBinaryOperator (BinaryOperator * bop) {
     VisitStmt(bop);
     //visit stmt in this binaryoperator:
     //from left tree and right tree 
     mEnv->binop(bop);
   }

//now only for *
   virtual void VisitUnaryOperator (UnaryOperator *unary){
    VisitStmt(unary);
    mEnv->unaryop(unary);
   }

   virtual void VisitDeclRefExpr(DeclRefExpr * expr) {
     VisitStmt(expr);
     mEnv->declref(expr);
   }
   virtual void VisitCastExpr(CastExpr * expr) {
     VisitStmt(expr);
     mEnv->cast(expr);
   }

//bind return value to callExpr
   virtual void VisitReturnStmt(ReturnStmt *ret){
      VisitStmt(ret);
      mEnv->visitReturn(ret);
   }


//for sizeof() in malloc
   virtual void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *uop)
   {

            VisitStmt(uop);
            mEnv->unarysizeof(uop);
   }

   virtual void VisitCallExpr(CallExpr * call) {
    // use this to get param’s value
     VisitStmt(call);
//bind param with value
     mEnv->call(call);

//get function body and visit it use code following
     FunctionDecl * callee = call->getDirectCallee(); 
     //can get function body
     if(callee){
      Stmt *body = callee->getBody();
      //visitStmt -> return stmt
      //builtin-function doesn't has body
      if(body && isa<CompoundStmt>(body)){
        VisitStmt(body);
      }
     }
   }

   virtual void VisitDeclStmt(DeclStmt * declstmt) {
    //printf("vistDeclStmt\n");
     mEnv->decl(declstmt);
   }

   virtual void VisitIntegerLiteral(IntegerLiteral *integer){
    //printf("visitintegerliteral\n");
    mEnv->integer(integer);
   }

   virtual Stmt * getWhileCompoundStmt(WhileStmt *whilestmt){
    //note: I have tried dyn_cast, but there is a error，
    //stmt and expr sometimes can convert
     Stmt* compound =  mEnv -> whilecompound(whilestmt);
     return compound;
   }

   virtual void VisitWhileStmt(WhileStmt *whilestmt){
   
   //first get body
    Stmt *body = getWhileCompoundStmt(whilestmt);
    //Stmt *condstmt = whilestmt->getCond();
    Expr *condStmt = whilestmt->getCond();
    Visit(condStmt);
    bool cond = mEnv->getcond(condStmt);
    //if cond satisfied ， into it
    while(cond){
      if(body && isa<CompoundStmt>(body)){
        VisitStmt(body);
      }

      Visit(condStmt);
      cond = mEnv->getcond(condStmt);
      //cout <<"in visitwhilestmt "<<"cond "<<cond;
    }

   }


// body and such as for(a=0,b=0;a<10;a=a+1) didn't handle
   virtual void VisitForStmt(ForStmt *forstmt){

     // Stmt *body = forstmt->getBody();
    //use Visit, we can get binaryop execute
      Expr *condexpr = forstmt->getCond();
      Visit(condexpr);
      bool cond = mEnv->getcond(condexpr);

      Stmt *init = forstmt->getInit();
//Visit function used to visit binaryoperator
      Visit(init);

      Expr  *inc= forstmt->getInc();

      while(cond){

        if(inc){
          Visit(inc);
        }
        Visit(condexpr);
        cond = mEnv->getcond(condexpr);
      }

   }

private:
   Environment * mEnv;
};

class InterpreterConsumer : public ASTConsumer {
public:
   explicit InterpreterConsumer(const ASTContext& context) : mEnv(),
       mVisitor(context, &mEnv) {
        //printf("InterpreterConsumer has started\n");
   }
   virtual ~InterpreterConsumer() {}

   virtual void HandleTranslationUnit(clang::ASTContext &Context) {
      //printf("start HandletranslationUnit\n");
     TranslationUnitDecl * decl = Context.getTranslationUnitDecl();
     mEnv.init(decl);

     FunctionDecl * entry = mEnv.getEntry();
     mVisitor.VisitStmt(entry->getBody());
  }
private:
   Environment mEnv;
   InterpreterVisitor mVisitor;
};

class InterpreterClassAction : public ASTFrontendAction {
public: 
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
    //printf("InterpreterClassAction has started\n");
    return std::unique_ptr<clang::ASTConsumer>(
        new InterpreterConsumer(Compiler.getASTContext()));
  }
};

int main (int argc, char ** argv) {
   if (argc > 1) {
      //printf("function starts\n");
       clang::tooling::runToolOnCode(new InterpreterClassAction, argv[1]);
   }
}
