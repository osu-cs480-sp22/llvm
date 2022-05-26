#include <iostream>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> TheBuilder(TheContext);
static llvm::Module* TheModule;

llvm::Value* numericConstant(float val) {
  return llvm::ConstantFP::get(TheContext, llvm::APFloat(val));
}

llvm::Value* binaryOperation(llvm::Value* lhs, llvm::Value* rhs, char op) {
  if (!lhs || !rhs) {
    return NULL;
  }

  switch (op) {
    case '+':
      return TheBuilder.CreateFAdd(lhs, rhs, "addtmp");
    case '-':
      return TheBuilder.CreateFSub(lhs, rhs, "subtmp");
    case '*':
      return TheBuilder.CreateFMul(lhs, rhs, "multmp");
    case '/':
      return TheBuilder.CreateFDiv(lhs, rhs, "divtmp");
    default:
      std::cerr << "Invalid operator: " << op << std::endl;
      return NULL;
  }
}

int main(int argc, char const *argv[]) {
  TheModule = new llvm::Module("LLVM_Demo_Program", TheContext);

  llvm::FunctionType* fooPrototype = llvm::FunctionType::get(
    llvm::Type::getFloatTy(TheContext), false
  );
  llvm::Function* foo = llvm::Function::Create(
    fooPrototype, llvm::GlobalValue::ExternalLinkage,
    "foo", TheModule
  );

  llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(
    TheContext, "entry", foo
  );
  TheBuilder.SetInsertPoint(entryBlock);

  llvm::Value* expr1 = binaryOperation(
    numericConstant(4),
    numericConstant(2),
    '*'
  );
  llvm::Value* expr2 = binaryOperation(
    numericConstant(8),
    expr1,
    '+'
  );

  // TheBuilder.CreateRetVoid();
  TheBuilder.CreateRet(expr2);

  llvm::verifyFunction(*foo);
  TheModule->print(llvm::outs(), NULL);

  return 0;
}
