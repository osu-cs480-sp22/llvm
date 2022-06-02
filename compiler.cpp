#include <iostream>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetMachine.h"


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

  llvm::Function* currFn = TheBuilder.GetInsertBlock()->getParent();
  llvm::BasicBlock* ifBlock = llvm::BasicBlock::Create(
    TheContext,
    "ifBlock",
    currFn
  );
  llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(
    TheContext,
    "elseBlock"
  );
  llvm::BasicBlock* continuationBlock = llvm::BasicBlock::Create(
    TheContext,
    "continuationBlock"
  );

  TheBuilder.CreateCondBr(cond, ifBlock, elseBlock);

  TheBuilder.SetInsertPoint(ifBlock);
  llvm::Value* aTimesB = binaryOperation(
    variableValue("a"),
    variableValue("b"),
    '*'
  );
  llvm::Value* ifBlockStmt = assignmentStatement("c", aTimesB);
  TheBuilder.CreateBr(continuationBlock);

  currFn->getBasicBlockList().push_back(elseBlock);
  TheBuilder.SetInsertPoint(elseBlock);
  llvm::Value* aPlusB = binaryOperation(
    variableValue("a"),
    variableValue("b"),
    '+'
  );
  llvm::Value* elseBlockStmt = assignmentStatement("c", aPlusB);
  TheBuilder.CreateBr(continuationBlock);

  currFn->getBasicBlockList().push_back(continuationBlock);
  TheBuilder.SetInsertPoint(continuationBlock);

  return continuationBlock;
}


void generateObjFile(std::string filename) {
  std::string targetTriple = llvm::sys::getDefaultTargetTriple();
  std::cerr << "Target triple: " << targetTriple << std::endl;

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  std::string err;
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTriple, err);
  if (!target) {
    std::cerr << "Error looking up target: " << err << std::endl;
    return;
  }

  std::string cpu = "generic";
  std::string features = "";
  llvm::TargetOptions options;
  llvm::TargetMachine* targetMachine = target->createTargetMachine(
    targetTriple,
    cpu,
    features,
    options,
    llvm::Optional<llvm::Reloc::Model>()
  );

  TheModule->setDataLayout(targetMachine->createDataLayout());
  TheModule->setTargetTriple(targetTriple);

  std::error_code ec;
  llvm::raw_fd_ostream fd(filename, ec, llvm::sys::fs::OF_None);
  if (ec) {
    std::cerr << "Error opening optput file: " << ec.message() << std::endl;
    return;
  }

  llvm::legacy::PassManager pm;
  targetMachine->addPassesToEmitFile(pm, fd, NULL, llvm::CodeGenFileType::CGFT_ObjectFile);
  pm.run(*TheModule);
  fd.close();
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

  llvm::Value* ifElse = ifElseStatement();

  // TheBuilder.CreateRetVoid();
  TheBuilder.CreateRet(variableValue("b"));

  llvm::verifyFunction(*foo);
  TheModule->print(llvm::outs(), NULL);

  generateObjFile("foo.o");

  return 0;
}
