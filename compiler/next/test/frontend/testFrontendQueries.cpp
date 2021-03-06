/*
 * Copyright 2021 Hewlett Packard Enterprise Development LP
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "chpl/frontend/frontend-queries.h"

#include "chpl/uast/Comment.h"
#include "chpl/uast/ModuleDecl.h"
#include "chpl/uast/VariableDecl.h"

// always check assertions in this test
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>

using namespace chpl;
using namespace uast;
using namespace frontend;

static void test0() {
  printf("test0\n");
  Context context;
  Context* ctx = &context;

  auto path = UniqueString::build(ctx, "input.chpl");
  std::string contents = "/* this is a test */";
  setFileText(ctx, path, contents);
 
  std::string gotContents = fileText(ctx, path);
  assert(gotContents == contents);
}

static void test1() {
  printf("test1\n");
  Context context;
  Context* ctx = &context;

  auto path = UniqueString::build(ctx, "input.chpl");
  std::string contents = "/* this is a test */";
  setFileText(ctx, path, contents);
 
  parse(ctx, path);
}

static const Module* parseOneModule(Context* ctx, UniqueString filepath) {
  const ModuleDeclVec& v = parse(ctx, filepath);
  assert(v.size() == 1);
  return v[0]->module();
}

static void test2() {
  printf("test2\n");
  Context context;
  Context* ctx = &context;

  ctx->advanceToNextRevision(true);
  auto modOnePath = UniqueString::build(ctx, "modOne.chpl");
  std::string modOneContents = "/* this is a test */\n"
                               "a;\n";
  setFileText(ctx, modOnePath, modOneContents);
  auto modTwoPath = UniqueString::build(ctx, "modTwo.chpl");
  std::string modTwoContents = "/* this is a another test */"
                               "a;\n";
  setFileText(ctx, modTwoPath, modTwoContents);
 
  const Module* moduleOne = nullptr;
  const Module* moduleTwo = nullptr;

  moduleOne = parseOneModule(ctx, modOnePath);
  moduleTwo = parseOneModule(ctx, modTwoPath);
  assert(moduleOne->numStmts() == 2);
  assert(moduleTwo->numStmts() == 2);
  ASTNode::dump(moduleOne);
  ASTNode::dump(moduleTwo);

  const Module* oldModuleOne = moduleOne;
  const Module* oldModuleTwo = moduleTwo;
  ctx->collectGarbage();

  printf("test2 changing whitespace in modOne.chpl\n");
  ctx->advanceToNextRevision(true);

  modOneContents = "/* this is a test */\n"
                   "\n"
                   "\n"
                   "a;\n";
  setFileText(ctx, modOnePath, modOneContents);
  setFileText(ctx, modTwoPath, modTwoContents);

  printf("test2 parsing modOne.chpl\n");
  moduleOne = parseOneModule(ctx, modOnePath);
  printf("test2 parsing modTwo.chpl\n");
  moduleTwo = parseOneModule(ctx, modTwoPath);
  assert(moduleOne->numStmts() == 2);
  assert(moduleTwo->numStmts() == 2);
  ASTNode::dump(moduleOne);
  ASTNode::dump(moduleTwo);

  // Check that the pointer values didn't change
  // (they shouldn't because we should have reused the modules).
  assert(moduleOne == oldModuleOne);
  assert(moduleTwo == oldModuleTwo);

  ctx->collectGarbage();
}

static void test3() {
  printf("test3\n");
  Context context;
  Context* ctx = &context;

  auto modulePath = UniqueString::build(ctx, "MyModule.chpl");
  const Module* module = nullptr;
  const Comment* comment = nullptr;
  const Identifier* identifierA = nullptr;
  const Identifier* identifierB = nullptr;
  const Block* block = nullptr;

  std::string moduleContents;

  moduleContents = "/* this is a test */\n"
                   "a;\n"
                   "b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);
  assert(module->numStmts() == 3);
  comment = module->stmt(0)->toComment();
  identifierA = module->stmt(1)->toIdentifier();
  identifierB = module->stmt(2)->toIdentifier();
  assert(comment);
  assert(identifierA);
  assert(identifierB);
  const Comment* oldComment = comment;
  const Identifier* oldIdentifierA = identifierA;
  const Identifier* oldIdentifierB = identifierB;

  printf("test3 adding Blocks\n");
  moduleContents = "/* this is a test */\n"
                   "{ x; }\n"
                   "a;\n"
                   "{ y; }\n"
                   "b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);

  // Check that the pointer values didn't change
  // (they shouldn't because we should have reused the parts that didn't change)
  assert(module->numStmts() == 5);
  comment = module->stmt(0)->toComment();
  block = module->stmt(1)->toBlock();
  identifierA = module->stmt(2)->toIdentifier();
  identifierB = module->stmt(4)->toIdentifier();
  assert(comment);
  assert(identifierA);
  assert(identifierB);
  assert(comment == oldComment);
  // These should not match because they have different IDs
  assert(identifierA != oldIdentifierA);
  assert(identifierB != oldIdentifierB);
  oldIdentifierA = identifierA;
  oldIdentifierB = identifierB;
  const Block* oldBlock = block;

  printf("test3 changing Identifier in Blocks\n");
  moduleContents = "/* this is a test */\n"
                   "{ xx; }\n"
                   "a;\n"
                   "{ yy; }\n"
                   "b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);

  // Check that the block and identifiers match
  assert(module->numStmts() == 5);
  comment = module->stmt(0)->toComment();
  block = module->stmt(1)->toBlock();
  identifierA = module->stmt(2)->toIdentifier();
  identifierB = module->stmt(4)->toIdentifier();
  assert(comment);
  assert(identifierA);
  assert(identifierB);
  assert(comment == oldComment);
  // This should not match because the contents changed
  assert(block != oldBlock);
  // These should match because they have the same contents and IDs
  assert(identifierA == oldIdentifierA);
  assert(identifierB == oldIdentifierB);
  oldBlock = nullptr; // it will be invalid after this point

  printf("test3 removing the Blocks\n");
  moduleContents = "/* this is a test */\n"
                   "a;\n"
                   "b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);

  // Check that the comment and identifiers match
  assert(module->numStmts() == 3);
  comment = module->stmt(0)->toComment();
  identifierA = module->stmt(1)->toIdentifier();
  identifierB = module->stmt(2)->toIdentifier();
  assert(comment);
  assert(identifierA);
  assert(identifierB);
  assert(comment == oldComment);
  // These should not match because they have different IDs
  assert(identifierA != oldIdentifierA);
  assert(identifierB != oldIdentifierB);
  oldIdentifierA = identifierA;
  oldIdentifierB = identifierB;

  printf("test3 replacing first Identifier\n");
  moduleContents = "/* this is a test */\n"
                   "aa;\n"
                   "b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);

  // Check that the comment and identifiers match
  assert(module->numStmts() == 3);
  comment = module->stmt(0)->toComment();
  identifierA = module->stmt(1)->toIdentifier();
  identifierB = module->stmt(2)->toIdentifier();
  assert(comment);
  assert(identifierA);
  assert(identifierB);
  assert(comment == oldComment);
  assert(identifierA != oldIdentifierA); // different contents
  assert(identifierB == oldIdentifierB);
}

static void test4() {
  printf("test4\n");
  Context context;
  Context* ctx = &context;

  auto modulePath = UniqueString::build(ctx, "MyModule.chpl");
  const Module* module = nullptr;
  const Comment* comment = nullptr;
  const VariableDecl* declA = nullptr;
  const VariableDecl* declB = nullptr;
  const Variable* A = nullptr;
  const Variable* B = nullptr;
  const Block* block = nullptr;

  std::string moduleContents;

  moduleContents = "/* this is a test */\n"
                   "var a;\n"
                   "var b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);
  assert(module->numStmts() == 3);
  comment = module->stmt(0)->toComment();
  declA = module->stmt(1)->toVariableDecl();
  declB = module->stmt(2)->toVariableDecl();
  assert(comment);
  assert(declA);
  assert(declB);
  A = declA->variable();
  B = declB->variable();
  assert(A);
  assert(B);
  const Module* oldModule = module;
  const Comment* oldComment = comment;
  const VariableDecl* oldDeclA = declA;
  const VariableDecl* oldDeclB = declB;
  const Variable* oldA = A;
  const Variable* oldB = B;

  printf("test4 adding Blocks\n");
  moduleContents = "/* this is a test */\n"
                   "{ var x; }\n"
                   "var a;\n"
                   "{ var y; }\n"
                   "var b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);

  // Check that the pointer values didn't change
  // (they shouldn't because we should have reused the parts that didn't change)
  assert(module->numStmts() == 5);
  comment = module->stmt(0)->toComment();
  block = module->stmt(1)->toBlock();
  declA = module->stmt(2)->toVariableDecl();
  declB = module->stmt(4)->toVariableDecl();
  assert(comment);
  assert(declA);
  assert(declB);
  A = declA->variable();
  B = declB->variable();
  assert(A);
  assert(B);

  // should not match because the contents changed
  assert(module != oldModule);
  oldModule = module;

  assert(comment == oldComment);
  // these should not match because they have different IDs
  assert(declA != oldDeclA);
  oldDeclA = declA;
  assert(declB != oldDeclB);
  oldDeclB = declB;
  // these should match though
  assert(A == oldA);
  assert(B == oldB);
  const Block* oldBlock = block;

  printf("test4 changing Identifier in Blocks\n");
  moduleContents = "/* this is a test */\n"
                   "{ var xx; }\n"
                   "var a;\n"
                   "{ var yy; }\n"
                   "var b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);

  // Check that the block and identifiers match
  assert(module->numStmts() == 5);
  comment = module->stmt(0)->toComment();
  block = module->stmt(1)->toBlock();
  declA = module->stmt(2)->toVariableDecl();
  declB = module->stmt(4)->toVariableDecl();
  assert(comment);
  assert(declA);
  assert(declB);
  A = declA->variable();
  B = declB->variable();
  assert(A);
  assert(B);

  // should not match because the contents changed
  assert(module != oldModule);
  oldModule = module;

  assert(comment == oldComment);
  assert(block != oldBlock); // should not match because contents changed
  // these have the same Id and should match
  assert(declA == oldDeclA);
  assert(declB == oldDeclB);
  assert(A == oldA);
  assert(B == oldB);

  printf("test4 removing the Blocks\n");
  moduleContents = "/* this is a test */\n"
                   "var a;\n"
                   "var b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);

  // Check that the comment and identifiers match
  assert(module->numStmts() == 3);
  comment = module->stmt(0)->toComment();
  declA = module->stmt(1)->toVariableDecl();
  declB = module->stmt(2)->toVariableDecl();
  assert(comment);
  assert(declA);
  assert(declB);
  A = declA->variable();
  B = declB->variable();
  assert(A);
  assert(B);

  // should not match because the contents changed
  assert(module != oldModule);
  oldModule = module;

  assert(comment == oldComment);
  // should not match because they have different IDs
  assert(declA != oldDeclA);
  oldDeclA = declA;
  assert(declB != oldDeclB);
  oldDeclB = declB;
  // these should match though.
  assert(A == oldA);
  assert(B == oldB);

  printf("test4 replacing first Decl\n");
  moduleContents = "/* this is a test */\n"
                   "var aa;\n"
                   "var b;\n";
  ctx->advanceToNextRevision(true);
  setFileText(ctx, modulePath, moduleContents);
  module = parseOneModule(ctx, modulePath);
  ctx->collectGarbage();

  ASTNode::dump(module);

  // Check that the comment and identifiers match
  assert(module->numStmts() == 3);
  comment = module->stmt(0)->toComment();
  declA = module->stmt(1)->toVariableDecl();
  declB = module->stmt(2)->toVariableDecl();
  assert(comment);
  assert(declA);
  assert(declB);
  A = declA->variable();
  B = declB->variable();
  assert(A);
  assert(B);

  // should not match because the contents changed
  assert(module != oldModule);
  oldModule = module;

  assert(comment == oldComment);

  assert(declA != oldDeclA); // contents changed
  assert(A != oldA); // name changed

  assert(declB == oldDeclB);
  assert(B == oldB);
}

static void test5() {
  printf("test5\n");
  Context context;
  Context* ctx = &context;

  auto modulePath = UniqueString::build(ctx, "MyModule.chpl");
  const Module* module = nullptr;
  const Comment* comment = nullptr;
  const VariableDecl* declA = nullptr;
  const VariableDecl* declB = nullptr;
  const Block* block = nullptr;

  std::string moduleContents;

  moduleContents = "/* this is a test */\n"
                   "var a;\n"
                   "var b;\n";

  // run the below several times to check that GC+reuse doesn't mess
  // anything up
  for (int i = 0; i < 3; i++) {
    ctx->advanceToNextRevision(true);
    setFileText(ctx, modulePath, moduleContents);
    module = parseOneModule(ctx, modulePath);

    ASTNode::dump(module);
    assert(module->numStmts() == 3);
    comment = module->stmt(0)->toComment();
    declA = module->stmt(1)->toVariableDecl();
    declB = module->stmt(2)->toVariableDecl();
    assert(comment);
    assert(declA);
    assert(declB);

    // Now check their locations
    Location commentLoc = locate(ctx, comment);
    Location declALoc = locate(ctx, declA);
    Location aLoc = locate(ctx, declA->variable());
    Location declBLoc = locate(ctx, declB);
    Location bLoc = locate(ctx, declB->variable());
    assert(commentLoc.path() == modulePath);
    assert(declALoc.path() == modulePath);
    assert(aLoc.path() == modulePath);
    assert(declBLoc.path() == modulePath);
    assert(bLoc.path() == modulePath);
    assert(commentLoc.line() == 1);
    assert(declALoc.line() == 2);
    assert(aLoc.line() == 2);
    assert(declBLoc.line() == 3);
    assert(bLoc.line() == 3);

    ctx->collectGarbage();
  }
}

int main() {
  test0();
  test1();
  test2();
  test3();
  test4();
  test5();

  return 0;
}
