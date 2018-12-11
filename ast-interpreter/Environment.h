//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//
#include <stdio.h>
#include<iostream>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace std;

class StackFrame {
   /// StackFrame maps Variable Declaration to Value
   /// Which are either integer or addresses (also represented using an Integer value)
   std::map<Decl*, int> mVars;
   std::map<Stmt*, int> mExprs;
   /// The current stmt
   Stmt * mPC;
public:
   StackFrame() : mVars(), mExprs(), mPC() {
   }

   void bindDecl(Decl* decl, int val) {
      mVars[decl] = val;
   }    
   int getDeclVal(Decl * decl) {
      assert (mVars.find(decl) != mVars.end());
      return mVars.find(decl)->second;
   }
   void bindStmt(Stmt * stmt, int val) {
	   mExprs[stmt] = val;
   }
   int getStmtVal(Stmt * stmt) {
	   assert (mExprs.find(stmt) != mExprs.end());
	   return mExprs[stmt];
   }
   void setPC(Stmt * stmt) {
	   mPC = stmt;
   }
   Stmt * getPC() {
	   return mPC;
   }
};

/
/// Heap maps address to a value
class Heap{
	//used to store address and address->value
	//for example: int *a; *a = 1;
	//or :for int **b; *b = a; maps address to address
	std::map<int, int> mContents;
public:
	Heap(): mContents(){
	}
// address is 4bytes
	int Malloc(int size){
		int *addrPointer = (int *)malloc(size); 
		//after malloc -> {addr: 0}
		int addr = *addrPointer;
		mContents[addr] = 0;
		return addr;
	} 
	void Free(int addr){
		int *addr1 = (int*)addr;
		//erase it in heap
		mContents.erase(addr);
		free(addr1);
	}

	int setHeap(int addr , int val){
		mContents[addr] = val;
	}
	int getHeap(int addr){
		assert(mContents.find(addr) != mContents.end());
		return mContents.find(addr) -> second;
	}

};



class Environment {
   std::vector<StackFrame> mStack;
   Heap mHeap;

   FunctionDecl * mFree;				/// Declartions to the built-in functions
   FunctionDecl * mMalloc;
   FunctionDecl * mInput;
   FunctionDecl * mOutput;

   FunctionDecl * mEntry;
public:
   /// Get the declartions to the built-in functions
   Environment() : mStack(), mFree(NULL), mMalloc(NULL), mInput(NULL), mOutput(NULL), mEntry(NULL) {
   }


   /// Initialize the Environment
   void init(TranslationUnitDecl * unit) {
	   for (TranslationUnitDecl::decl_iterator i =unit->decls_begin(), e = unit->decls_end(); i != e; ++ i) {
		   if (FunctionDecl * fdecl = dyn_cast<FunctionDecl>(*i) ) {
			   if (fdecl->getName().equals("FREE")) mFree = fdecl;
			   else if (fdecl->getName().equals("MALLOC")) mMalloc = fdecl;
			   else if (fdecl->getName().equals("GET")) mInput = fdecl;
			   else if (fdecl->getName().equals("PRINT")) mOutput = fdecl;
			   else if (fdecl->getName().equals("main")) {mEntry = fdecl; 
			   	//printf("entry  =  main()\n");
			   }
		   }
	   }
	   mStack.push_back(StackFrame());
	   //global variable need store in stackframe
	   for (TranslationUnitDecl::decl_iterator i =unit->decls_begin(), e = unit->decls_end(); i != e; ++ i){
	   	// iterator -> pointer *i means get value
	   		//(*i) -> dump();
	   		if (VarDecl *var = dyn_cast<VarDecl>(*i) ){
	   			if (var->hasInit()){
	   				Expr *init = var->getInit();
   					if(IntegerLiteral *integer = dyn_cast<IntegerLiteral>(init)){
   						int val = integer -> getValue().getSExtValue();
   						mStack.back().bindDecl(var,val);
	   				}
	   			}else{
	   				mStack.back().bindDecl(var,0);
	   			}
	   		}
	   }

   }

   FunctionDecl * getEntry() {
	   return mEntry;
   }

   /// !TODO Support comparison operation
   void binop(BinaryOperator *bop) {
   	   //printf("env binaryOperator running\n");
	   Expr * left = bop->getLHS();
	   Expr * right = bop->getRHS();


	   if (bop->isAssignmentOp()) {
//1: a = b
//2: *a = Malloc(sizeof(int))
//3: *a = b -> read from heap
	   	if(isa<UnaryOperator>(left)){
	   		UnaryOperator *uop = dyn_cast<UnaryOperator>(left);
	   		if((uop->getOpcode()) == UO_Deref){
	   			Expr *expr= uop->getSubExpr();
	   			int addr = mStack.back().getStmtVal(expr);
	   			int val = mStack.back().getStmtVal(right);
	   			mHeap.setHeap(addr,val);
	   		}
	   	}
	   	else{
	   	//pointer -> PointerType
	   	//BuiltinType -> int,char 
		   int val = mStack.back().getStmtVal(right);
		   mStack.back().bindStmt(left, val);
		   if (DeclRefExpr * declexpr = dyn_cast<DeclRefExpr>(left)) {
			   Decl * decl = declexpr->getFoundDecl();
			   mStack.back().bindDecl(decl, val);
		   }
		}
	   }
	   else if(bop->isAdditiveOp()){
	   	// + -> visitstmt so we can bind it 
	   	// |-ImplicitCastExpr 0x72620b8 <col:10> 'int' <LValueToRValue>
     //    | `-DeclRefExpr 0x7262068 <col:10> 'int' lvalue Var 0x7261f38 'a' 'int'
     //    `-ImplicitCastExpr 0x72620d0 <col:12> 'int' <LValueToRValue>
     //      `-DeclRefExpr 0x7262090 <col:12> 'int' lvalue Var 0x7261ea8 'b' 'int'

	   	int valLeft = mStack.back().getStmtVal(left);
	   	int valRight;
	   	if(isa<IntegerLiteral>(right)){
	   		IntegerLiteral *integer = dyn_cast<IntegerLiteral>(right);
	   		valRight = integer -> getValue().getSExtValue();
	   	}else{
	   		valRight = mStack.back().getStmtVal(right);
	   	}

	   	// + / -
	   	switch(bop->getOpcode()){

	   		case BO_Add:
	   		//cout <<"add"<<valRight+valLeft<<endl;
	   		mStack.back().bindStmt(bop,valLeft+valRight);
	   		break;

	   		case BO_Sub:
	   		mStack.back().bindStmt(bop,valLeft-valRight);
	   		break;
	   	}
	   }
	   else if(bop->isMultiplicativeOp()){

		int valLeft = mStack.back().getStmtVal(left);
	   	int valRight;
	   	if(isa<IntegerLiteral>(right)){
	   		IntegerLiteral *integer = dyn_cast<IntegerLiteral>(right);
	   		valRight = integer -> getValue().getSExtValue();
	   	}else{
	   		valRight = mStack.back().getStmtVal(right);
	   	}

	   	switch(bop -> getOpcode()){
	   		case BO_Mul:
	   		//bop->dump();
	   		//cout<<"a*b "<<valLeft*valRight<<endl;
	   		mStack.back().bindStmt(bop,valLeft*valRight);
	   		break;

	   		case BO_Div:
	   		assert(valRight != 0);
	   		mStack.back().bindStmt(bop, valLeft / valRight);
	   		break;
	   	}
	   }

	   else if(bop -> isRelationalOp()){
	   	int valLeft = mStack.back().getStmtVal(left);
	   	int valRight;
	   	if(isa<IntegerLiteral>(right)){
	   		IntegerLiteral *integer = dyn_cast<IntegerLiteral>(right);
	   		valRight = integer -> getValue().getSExtValue();
	   	}else{
	   		valRight = mStack.back().getStmtVal(right);
	   	}

	   	switch(bop -> getOpcode()){

	   		//<
	   		case BO_LT:
	   		if(valLeft < valRight){
	   			mStack.back().bindStmt(bop, true);
	   			//cout <<"true"<<endl;
	   		}else{
	   			mStack.back().bindStmt(bop, false);
	   			//cout<<"false"<<endl;
	   		}
	   		break;

	   		//>
	   		case BO_GT:
	   		if( valLeft > valRight )
	   			mStack.back().bindStmt(bop,true);
	   		else
	   			mStack.back().bindStmt(bop,false);
	   		break;

	   		//>=
	   		case BO_GE:
	   		if( valLeft >= valRight )
	   			mStack.back().bindStmt(bop,true);
	   		else
	   			mStack.back().bindStmt(bop,false);
	   		break;

	   		//<=
	   		case BO_LE:
	   		if( valLeft <= valRight )
	   			mStack.back().bindStmt(bop,true);
	   		else
	   			mStack.back().bindStmt(bop,false);
	   		break;

	   		default:
	   		cout<<" invalid input comparisons! "<<endl;
	   		break;
	   	}

	   }

	   else if(bop -> isEqualityOp()){

	   	int valLeft = mStack.back().getStmtVal(left);
	   	int valRight;
	   	if(isa<IntegerLiteral>(right)){
	   		IntegerLiteral *integer = dyn_cast<IntegerLiteral>(right);
	   		valRight = integer -> getValue().getSExtValue();
	   	}else{
	   		valRight = mStack.back().getStmtVal(right);
	   	}

	   	switch(bop -> getOpcode()){
	   	    case BO_EQ:
	   		if( valLeft == valRight )
	   			mStack.back().bindStmt(bop,true);
	   		else
	   			mStack.back().bindStmt(bop,false);
	   		break;

	   		//!=
	   		case BO_NE:
	   		if( valLeft != valRight )
	   			mStack.back().bindStmt(bop,true);
	   		else
	   			mStack.back().bindStmt(bop,false);
	   		break;

	   		default:
	   		cout<<" invalid input comparisons! "<<endl;
	   		break;
	   }
	}
	   // how to stand comparision symbols
   }

   void unaryop(UnaryOperator *unary){
   	//get defer var -> *
   	Expr *expr = unary->getSubExpr();
   	int val = mStack.back().getStmtVal(expr);
   	switch(unary->getOpcode()){
   		case UO_Deref:
  		mStack.back().bindStmt(unary,mHeap.getHeap(val));
  		break;

   	}
   }


//bind integer in mStack 
   void integer(IntegerLiteral *integer){
   		int val = integer->getValue().getSExtValue();
   		mStack.back().bindStmt(integer, val);
   }

//function decl , pointer decl(int *a) and integer decl
   void decl(DeclStmt * declstmt) {
	   for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end();
			   it != ie; ++ it) {
		   Decl * decl = *it;
        //a = 1
		   if (VarDecl * vardecl = dyn_cast<VarDecl>(decl)) {
		   	   	if (vardecl->hasInit()){
		   	   		Expr *init = vardecl->getInit();
		   	   		if(IntegerLiteral *integer = dyn_cast<IntegerLiteral>(init)){
		   	   			int val = integer->getValue().getSExtValue();
		   	   			//cout << "output val"<<val<<endl;
		   	   			mStack.back().bindDecl(vardecl, val);	
	
		   	   		}

		   	   	} else {
		   	   		// int a
		   	   		mStack.back().bindDecl(vardecl, 0);	
		   	   	}
		   }
	   }
   }
   void declref(DeclRefExpr * declref) {
   	//printf("env declref running\n");
	   mStack.back().setPC(declref);
	   if (declref->getType()->isIntegerType()) {
		   Decl* decl = declref->getFoundDecl();
		   int val = mStack.back().getDeclVal(decl);
		   mStack.back().bindStmt(declref, val);
	   }
//DeclRefExpr 0x20afd58 'void (int)' lvalue Function 0x20afaa0 'PRINT' 'void (int)'
//need to be initial
	   else if(declref->getType()->isPointerType()){
	   		Decl *decl = declref->getFoundDecl();
// FunctionDecl 0x3488aa0 <input.cc:4:1, col:22> col:13 used PRINT 'void (int)' extern
// `-ParmVarDecl 0x34889d8 <col:19> col:22 'int'
//->type:
//FunctionProtoType 0x3618a40 'void (int)' cdecl
// |-BuiltinType 0x35e2510 'void'
// `-BuiltinType 0x35e25b0 'int
//pointer type -> declref get right decl
	   		int val = mStack.back().getDeclVal(decl);
	   		mStack.back().bindStmt(declref, val);
	   }else{
	   	//for functions 
	   	Decl *decl = declref->getFoundDecl();
	   	int val = 0;
	   	mStack.back().bindStmt(declref,val);
	   }
   }

   void cast(CastExpr * castexpr) {
   	//printf("env cast running\n");
	   mStack.back().setPC(castexpr);
	   if (castexpr->getType()->isIntegerType()) {
		   Expr * expr = castexpr->getSubExpr();
		   int val = mStack.back().getStmtVal(expr);
		   mStack.back().bindStmt(castexpr, val );
	   }
// ImplicitCastExpr 0x63f4180 <col:12, col:24> 'int' <IntegralCast>
//     |     `-BinaryOperator 0x63f40e0 <col:12, col:24> 'unsigned long' '*'
//     |       |-UnaryExprOrTypeTraitExpr 0x63f4088 <col:12, col:22> 'unsigned long' sizeof 'int'
//     |       `-ImplicitCastExpr 0x63f40c8 <col:24> 'unsigned long' <IntegralCast>
//     |         `-IntegerLiteral 0x63f40a8 <col:24> 'int' 
//builtin type??
//function ->pointertype 
//deal with function decl initial in declref
	   else{
//function and pointer in this cannot be different
	   		//cout<<"not integer type cast"<<endl;
	   		Expr * expr = castexpr->getSubExpr();
	   		//expr->dump();
		   int val = mStack.back().getStmtVal(expr);
		   mStack.back().bindStmt(castexpr, val );
	   }
   }

    Stmt* whilecompound(WhileStmt *whilestmt){
	   	//cout << "into whilecompound"<<endl;
	   	Stmt *body= whilestmt->getBody();
	   	return body;
   }

   bool getcond(Expr *expr){
   		return mStack.back().getStmtVal(expr);
   }

   void visitReturn(ReturnStmt *ret){
   	Expr *value = ret->getRetValue();
   	//avoid return 0
   	int val = 0;
   	val=mStack.back().getStmtVal(value);
   	
   	//return cannot be visited use call or functiondecl
   	//I needn't bind it

   	mStack.pop_back();
   	Stmt *pc = mStack.back().getPC();
   	//pc->dump();
   	//pc here used to point to callfunction
   	mStack.back().bindStmt(pc,val);
   }


//deal with sizeof()
//test:use a fixed value to handle it ,don't achive malloc
// only use a address so hrer can bind a arbitary value
   void unarysizeof(UnaryExprOrTypeTraitExpr *uop){
   	UnaryExprOrTypeTrait typetrait = uop -> getKind();
   	int val;
   	switch(typetrait){
   		case UETT_SizeOf:
   		if(uop->getArgumentType ()->isPointerType()){
   			val = sizeof(int *);
   		}
   		//this is a sizeof(int),sizeof(char)
   		else{
   			val = sizeof(long);
   		}
   	}
   	mStack.back().bindStmt(uop, val);
   }


   /// !TODO Support Function Call
   void call(CallExpr * callexpr ) {
   	//printf("env call running\n");
	   mStack.back().setPC(callexpr);
	   int val = 0;
	   FunctionDecl * callee = callexpr->getDirectCallee();
	   if (callee == mInput) {
		  llvm::errs() << "Please Input an Integer Value : ";
		  scanf("%d", &val);

		  mStack.back().bindStmt(callexpr, val);
	   } else if (callee == mOutput) {
		   Expr * decl = callexpr->getArg(0);
		   val = mStack.back().getStmtVal(decl);
		   llvm::errs() << val;
	   } else if(callee == mMalloc){

	   		//cout <<"in malloc function"<<endl;
	   		//use 0 to initialize
	   		Expr *expr = callexpr->getArg(0);
	   		//buf -> address of malloc area
	   		int buf = mHeap.Malloc(val);
	   		mStack.back().bindStmt(callexpr,buf);
	   		
	   } else if(callee == mFree){
	   		//cout<<"in free function"<<endl;
	   		Expr *expr = callexpr->getArg(0);
	   		//val is an address
	   		//expr->dump();
	   		val = mStack.back().getStmtVal(expr);
	   		mHeap.Free(val);
	   		mStack.back().bindStmt(callexpr, 0);
	   }
	   else {
	   	//note:
		   /// You could add your code here for Function call Return
	   		//Expr *decl = callexpr ->getArg();
	   		StackFrame s;
	   		//cout << "function into here";
	   		int funcparam = callee -> getNumParams();
	   		//function body return and visitStmt body and finally getvalue ??? 
	   		int argnum = callexpr->getNumArgs();
	   		vector<int> varvec; 
	   		for(int i=0;i<argnum;i++){
	   			Expr *decl = callexpr->getArg(i);
	   			val = mStack.back().getStmtVal(decl);
	   			varvec.push_back(val);
	   		}
	   		//cout<<"function param"<<endl;
	   		for(int i=0;i<funcparam;i++){
	   			ParmVarDecl* decl = callee->getParamDecl(i);
	   			int paramval = varvec[i];
	   			s.bindDecl(decl, paramval);
	   			//decl->dump();
	   			//cout<<varvec[i]<<endl;
	   		}

	   		mStack.push_back(s);
	   }
   }
};
