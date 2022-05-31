#include <iostream>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> TheBuilder(TheContext);
static llvm::Module* TheModule;

static std::map<std::string, llvm::Value*> TheSymbolTable;

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
    case '<':
      lhs = TheBuilder.CreateFCmpULT(lhs, rhs, "lttmp");
      return TheBuilder.CreateUIToFP(
        lhs,
        llvm::Type::getFloatTy(TheContext),
        "booltmp"
      );
    default:
      std::cerr << "Invalid operator: " << op << std::endl;
      return NULL;
  }
}

llvm::Value* generateEntryBlockAlloca(std::string name) {
  llvm::Function* currFn = TheBuilder.GetInsertBlock()->getParent();
  llvm::IRBuilder<> tmpBuilder(
    &currFn->getEntryBlock(),
    currFn->getEntryBlock().begin()
  );
  return tmpBuilder.CreateAlloca(
    llvm::Type::getFloatTy(TheContext), 0, name.c_str()
  );
}

llvm::Value* assignmentStatement(std::string lhs, llvm::Value* rhs) {
  if (!rhs) {
    return NULL;
  }

  if (!TheSymbolTable.count(lhs)) {
    TheSymbolTable[lhs] = generateEntryBlockAlloca(lhs);
  }
  return TheBuilder.CreateStore(rhs, TheSymbolTable[lhs]);
}

llvm::Value* variableValue(std::string name) {
  llvm::Value* ptr = TheSymbolTable[name];
  if (!ptr) {
    std::cerr << "Unknown variable name: " << name << std::endl;
    return NULL;
  }
  return TheBuilder.CreateLoad(
    llvm::Type::getFloatTy(TheContext),
    ptr,
    name.c_str()
  );
}

llvm::Value* ifElseStatement() {
  llvm::Value* cond = binaryOperation(
    variableValue("b"),
    numericConstant(8),
    '<'
  );
  cond = TheBuilder.CreateFCmpONE(
    cond, numericConstant(0), "ifcond"
  );
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

  llvm::Value* assign1 = assignmentStatement("a", expr2);

  llvm::Value* expr3 = binaryOperation(
    variableValue("a"),
    numericConstant(4),
    '/'
  );
  llvm::Value* assign2 = assignmentStatement("b", expr3);

  // TheBuilder.CreateRetVoid();
  TheBuilder.CreateRet(variableValue("b"));

  llvm::verifyFunction(*foo);
  TheModule->print(llvm::outs(), NULL);

  return 0;
}
