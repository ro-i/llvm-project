//===- TestOMPTraits.cpp - Tests for OMP Trait classes -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "kmp_traits.h"
#include "gtest/gtest.h"

using namespace kmp_trait;

namespace {

//===----------------------------------------------------------------------===//
// OMPWildcardTrait Tests
//===----------------------------------------------------------------------===//

TEST(OMPWildcardTraitTest, MatchesAnyDevice) {
  OMPWildcardTrait *Trait = new OMPWildcardTrait();

  EXPECT_TRUE(Trait->match(0));
  EXPECT_TRUE(Trait->match(1));
  EXPECT_TRUE(Trait->match(100));
  EXPECT_TRUE(Trait->match(-1));

  delete Trait;
}

TEST(OMPWildcardTraitTest, Equality) {
  OMPWildcardTrait *T1 = new OMPWildcardTrait();
  OMPWildcardTrait *T2 = new OMPWildcardTrait();

  EXPECT_TRUE(*T1 == *T2);

  delete T1;
  delete T2;
}

//===----------------------------------------------------------------------===//
// OMPLiteralTrait Tests
//===----------------------------------------------------------------------===//

TEST(OMPLiteralTraitTest, MatchesExactDevice) {
  OMPLiteralTrait *Trait = new OMPLiteralTrait(5);

  EXPECT_TRUE(Trait->match(5));
  EXPECT_FALSE(Trait->match(0));
  EXPECT_FALSE(Trait->match(4));
  EXPECT_FALSE(Trait->match(6));

  delete Trait;
}

TEST(OMPLiteralTraitTest, MatchesZero) {
  OMPLiteralTrait *Trait = new OMPLiteralTrait(0);

  EXPECT_TRUE(Trait->match(0));
  EXPECT_FALSE(Trait->match(1));

  delete Trait;
}

TEST(OMPLiteralTraitTest, MatchesNegative) {
  OMPLiteralTrait *Trait = new OMPLiteralTrait(-1);

  EXPECT_TRUE(Trait->match(-1));
  EXPECT_FALSE(Trait->match(0));
  EXPECT_FALSE(Trait->match(1));

  delete Trait;
}

TEST(OMPLiteralTraitTest, EqualitySameValue) {
  OMPLiteralTrait *T1 = new OMPLiteralTrait(42);
  OMPLiteralTrait *T2 = new OMPLiteralTrait(42);

  EXPECT_TRUE(*T1 == *T2);

  delete T1;
  delete T2;
}

TEST(OMPLiteralTraitTest, EqualityDifferentValue) {
  OMPLiteralTrait *T1 = new OMPLiteralTrait(1);
  OMPLiteralTrait *T2 = new OMPLiteralTrait(2);

  EXPECT_FALSE(*T1 == *T2);

  delete T1;
  delete T2;
}

//===----------------------------------------------------------------------===//
// OMPUIDTrait Tests
//===----------------------------------------------------------------------===//

TEST(OMPUIDTraitTest, Construction) {
  OMPUIDTrait *Trait = new OMPUIDTrait(StringRef("test-uid"));

  // Just verify it can be constructed without crashing
  delete Trait;
}

TEST(OMPUIDTraitTest, MatchWithMock) {
  OMPUIDTrait *Trait = new OMPUIDTrait(StringRef("device-0"));

  // Uses the mock omp_get_uid_from_device
  EXPECT_TRUE(Trait->match(0)); // device-0 matches
  EXPECT_FALSE(Trait->match(1)); // device-1 doesn't match
  EXPECT_FALSE(Trait->match(2)); // device-2 doesn't match

  delete Trait;
}

TEST(OMPUIDTraitTest, MatchWithCustomMock) {
  OMPUIDTrait *Trait = new OMPUIDTrait(StringRef("custom-uid"));

  // Set a custom mock function
  Trait->setUIDFromDevice([](int Device) -> const char * {
    return Device == 2 ? "custom-uid" : "other";
  });

  EXPECT_FALSE(Trait->match(0));
  EXPECT_FALSE(Trait->match(1));
  EXPECT_TRUE(Trait->match(2)); // custom-uid matches device 2
  EXPECT_FALSE(Trait->match(3));

  delete Trait;
}

TEST(OMPUIDTraitTest, EqualitySameUID) {
  OMPUIDTrait *T1 = new OMPUIDTrait(StringRef("my-device"));
  OMPUIDTrait *T2 = new OMPUIDTrait(StringRef("my-device"));

  EXPECT_TRUE(*T1 == *T2);

  delete T1;
  delete T2;
}

TEST(OMPUIDTraitTest, EqualityDifferentUID) {
  OMPUIDTrait *T1 = new OMPUIDTrait(StringRef("device-a"));
  OMPUIDTrait *T2 = new OMPUIDTrait(StringRef("device-b"));

  EXPECT_FALSE(*T1 == *T2);

  delete T1;
  delete T2;
}

//===----------------------------------------------------------------------===//
// OMPTraitExprSingle Tests
//===----------------------------------------------------------------------===//

TEST(OMPTraitExprSingleTest, CreateAndDestroy) {
  OMPTraitExprSingle *Expr = new OMPTraitExprSingle();
  EXPECT_NE(Expr, nullptr);
  delete Expr;
}

TEST(OMPTraitExprSingleTest, CreateWithTrait) {
  OMPTraitExprSingle *Expr = new OMPTraitExprSingle(new OMPLiteralTrait(2));

  // Mock: 4 devices
  Expr->setNumDevices([]() { return 4; });

  EXPECT_TRUE(Expr->match(2));
  EXPECT_FALSE(Expr->match(0));
  EXPECT_FALSE(Expr->match(1));
  EXPECT_FALSE(Expr->match(5)); // Out of range

  delete Expr;
}

TEST(OMPTraitExprSingleTest, SetTrait) {
  OMPTraitExprSingle *Expr = new OMPTraitExprSingle();
  Expr->setTrait(new OMPLiteralTrait(3));

  // Mock: 4 devices
  Expr->setNumDevices([]() { return 4; });

  EXPECT_TRUE(Expr->match(3));
  EXPECT_FALSE(Expr->match(0));

  delete Expr;
}

TEST(OMPTraitExprSingleTest, DefaultNotNegated) {
  OMPTraitExprSingle *Expr = new OMPTraitExprSingle();

  EXPECT_FALSE(Expr->isNegated());

  delete Expr;
}

TEST(OMPTraitExprSingleTest, SetNegated) {
  OMPTraitExprSingle *Expr = new OMPTraitExprSingle();

  Expr->setNegated(true);
  EXPECT_TRUE(Expr->isNegated());

  Expr->setNegated(false);
  EXPECT_FALSE(Expr->isNegated());

  delete Expr;
}

TEST(OMPTraitExprSingleTest, MatchNegated) {
  OMPTraitExprSingle *Expr = new OMPTraitExprSingle(new OMPLiteralTrait(2));
  Expr->setNegated(true);

  // Mock: 4 devices
  Expr->setNumDevices([]() { return 4; });

  // Without negation: matches 2
  // With negation: matches everything in-range except 2
  EXPECT_FALSE(Expr->match(2));
  EXPECT_TRUE(Expr->match(0));
  EXPECT_TRUE(Expr->match(1));
  EXPECT_TRUE(Expr->match(3));
  // Out of range devices return false regardless of negation
  EXPECT_FALSE(Expr->match(5));

  delete Expr;
}

TEST(OMPTraitExprSingleTest, MatchWildcard) {
  OMPTraitExprSingle *Expr = new OMPTraitExprSingle(new OMPWildcardTrait());

  // Mock: 4 devices
  Expr->setNumDevices([]() { return 4; });

  // Wildcard matches any in-range device
  EXPECT_TRUE(Expr->match(0));
  EXPECT_TRUE(Expr->match(3));
  // Out of range devices return false
  EXPECT_FALSE(Expr->match(100));

  delete Expr;
}

TEST(OMPTraitExprSingleTest, Evaluate) {
  OMPTraitExprSingle *Expr = new OMPTraitExprSingle(new OMPLiteralTrait(1));

  // Mock: 3 devices
  Expr->setNumDevices([]() { return 3; });

  Vector<int> Result = Expr->evaluate();
  EXPECT_EQ(Result.size(), 1u);
  EXPECT_TRUE(Result.contains(1));
  EXPECT_FALSE(Result.contains(0));
  EXPECT_FALSE(Result.contains(2));

  delete Expr;
}

TEST(OMPTraitExprSingleTest, Equality) {
  OMPTraitExprSingle *E1 = new OMPTraitExprSingle(new OMPLiteralTrait(1));
  OMPTraitExprSingle *E2 = new OMPTraitExprSingle(new OMPLiteralTrait(1));

  EXPECT_TRUE(*E1 == *E2);

  delete E1;
  delete E2;
}

TEST(OMPTraitExprSingleTest, EqualityDifferentTrait) {
  OMPTraitExprSingle *E1 = new OMPTraitExprSingle(new OMPLiteralTrait(1));
  OMPTraitExprSingle *E2 = new OMPTraitExprSingle(new OMPLiteralTrait(2));

  EXPECT_FALSE(*E1 == *E2);

  delete E1;
  delete E2;
}

TEST(OMPTraitExprSingleTest, EqualityDifferentNegation) {
  OMPTraitExprSingle *E1 = new OMPTraitExprSingle(new OMPLiteralTrait(1));
  OMPTraitExprSingle *E2 = new OMPTraitExprSingle(new OMPLiteralTrait(1));
  E2->setNegated(true);

  EXPECT_FALSE(*E1 == *E2);

  delete E1;
  delete E2;
}

//===----------------------------------------------------------------------===//
// OMPTraitExprGroup Tests
//===----------------------------------------------------------------------===//

TEST(OMPTraitExprGroupTest, CreateAndDestroy) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();
  EXPECT_NE(Group, nullptr);
  delete Group;
}

TEST(OMPTraitExprGroupTest, DefaultTypeIsOR) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();

  EXPECT_EQ(Group->getGroupType(), OMPTraitExprGroup::OR);

  delete Group;
}

TEST(OMPTraitExprGroupTest, SetTypeAND) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();

  Group->setGroupType(OMPTraitExprGroup::AND);
  EXPECT_EQ(Group->getGroupType(), OMPTraitExprGroup::AND);

  delete Group;
}

TEST(OMPTraitExprGroupTest, DefaultNotNegated) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();

  EXPECT_FALSE(Group->isNegated());

  delete Group;
}

TEST(OMPTraitExprGroupTest, SetNegated) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();

  Group->setNegated(true);
  EXPECT_TRUE(Group->isNegated());

  Group->setNegated(false);
  EXPECT_FALSE(Group->isNegated());

  delete Group;
}

TEST(OMPTraitExprGroupTest, AddTraitDirectly) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();

  Group->addExpr(new OMPWildcardTrait());

  // Mock: 4 devices
  Group->setNumDevices([]() { return 4; });

  // Wildcard matches any in-range device
  EXPECT_TRUE(Group->match(0));
  EXPECT_TRUE(Group->match(3));
  // Out of range devices return false
  EXPECT_FALSE(Group->match(100));

  delete Group;
}

TEST(OMPTraitExprGroupTest, AddExpr) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();

  Group->addExpr(new OMPTraitExprSingle(new OMPLiteralTrait(2)));

  // Mock: 4 devices
  Group->setNumDevices([]() { return 4; });

  EXPECT_TRUE(Group->match(2));
  EXPECT_FALSE(Group->match(0));
  EXPECT_FALSE(Group->match(5)); // Out of range

  delete Group;
}

TEST(OMPTraitExprGroupTest, MatchORSemantics) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();
  Group->setGroupType(OMPTraitExprGroup::OR);

  Group->addExpr(new OMPLiteralTrait(1));
  Group->addExpr(new OMPLiteralTrait(2));
  Group->addExpr(new OMPLiteralTrait(3));

  // Mock: 5 devices
  Group->setNumDevices([]() { return 5; });

  // OR: matches if ANY trait matches
  EXPECT_TRUE(Group->match(1));
  EXPECT_TRUE(Group->match(2));
  EXPECT_TRUE(Group->match(3));
  EXPECT_FALSE(Group->match(0));
  EXPECT_FALSE(Group->match(4));

  delete Group;
}

TEST(OMPTraitExprGroupTest, MatchANDSemantics) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();
  Group->setGroupType(OMPTraitExprGroup::AND);

  // For AND to pass, ALL traits must match the same device
  // A single literal only matches one device
  Group->addExpr(new OMPLiteralTrait(2));

  // Mock: 4 devices
  Group->setNumDevices([]() { return 4; });

  EXPECT_TRUE(Group->match(2));
  EXPECT_FALSE(Group->match(0));
  // Out of range
  EXPECT_FALSE(Group->match(5));

  delete Group;
}

TEST(OMPTraitExprGroupTest, MatchANDWithWildcard) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();
  Group->setGroupType(OMPTraitExprGroup::AND);

  Group->addExpr(new OMPWildcardTrait());
  Group->addExpr(new OMPLiteralTrait(2));

  // Mock: 4 devices
  Group->setNumDevices([]() { return 4; });

  // Wildcard matches all, literal matches 2
  // AND: both must match
  EXPECT_TRUE(Group->match(2));
  EXPECT_FALSE(Group->match(0));
  // Out of range
  EXPECT_FALSE(Group->match(5));

  delete Group;
}

TEST(OMPTraitExprGroupTest, MatchNegated) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();

  Group->addExpr(new OMPLiteralTrait(2));
  Group->setNegated(true);

  // Mock: 4 devices
  Group->setNumDevices([]() { return 4; });

  // Without negation: matches 2
  // With negation: matches everything in-range except 2
  EXPECT_FALSE(Group->match(2));
  EXPECT_TRUE(Group->match(0));
  EXPECT_TRUE(Group->match(1));
  EXPECT_TRUE(Group->match(3));
  // Out of range devices return false regardless of negation
  EXPECT_FALSE(Group->match(5));

  delete Group;
}

TEST(OMPTraitExprGroupTest, MatchEmptyGroupOR) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();
  Group->setGroupType(OMPTraitExprGroup::OR);

  // Mock: 4 devices
  Group->setNumDevices([]() { return 4; });

  // Empty OR: no traits match, so result is false
  EXPECT_FALSE(Group->match(0));
  EXPECT_FALSE(Group->match(1));

  delete Group;
}

TEST(OMPTraitExprGroupTest, MatchEmptyGroupAND) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();
  Group->setGroupType(OMPTraitExprGroup::AND);

  // Mock: 4 devices
  Group->setNumDevices([]() { return 4; });

  // Empty AND: vacuously true (0 out of 0 traits match)
  EXPECT_TRUE(Group->match(0));
  EXPECT_TRUE(Group->match(1));

  delete Group;
}

TEST(OMPTraitExprGroupTest, EvaluateWithMock) {
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();

  // Mock: 3 devices
  Group->setNumDevices([]() { return 3; });

  Group->addExpr(new OMPLiteralTrait(0));
  Group->addExpr(new OMPLiteralTrait(2));

  Vector<int> Result = Group->evaluate();
  EXPECT_EQ(Result.size(), 2u);
  EXPECT_TRUE(Result.contains(0));
  EXPECT_TRUE(Result.contains(2));
  EXPECT_FALSE(Result.contains(1));

  delete Group;
}

TEST(OMPTraitExprGroupTest, Equality) {
  OMPTraitExprGroup *G1 = new OMPTraitExprGroup();
  OMPTraitExprGroup *G2 = new OMPTraitExprGroup();

  G1->addExpr(new OMPLiteralTrait(1));
  G2->addExpr(new OMPLiteralTrait(1));

  EXPECT_TRUE(*G1 == *G2);

  delete G1;
  delete G2;
}

TEST(OMPTraitExprGroupTest, EqualityDifferentNegation) {
  OMPTraitExprGroup *G1 = new OMPTraitExprGroup();
  OMPTraitExprGroup *G2 = new OMPTraitExprGroup();

  G1->addExpr(new OMPLiteralTrait(1));
  G2->addExpr(new OMPLiteralTrait(1));
  G2->setNegated(true);

  EXPECT_FALSE(*G1 == *G2);

  delete G1;
  delete G2;
}

TEST(OMPTraitExprGroupTest, NestedGroups) {
  OMPTraitExprGroup *Outer = new OMPTraitExprGroup();
  Outer->setGroupType(OMPTraitExprGroup::OR);

  OMPTraitExprGroup *Inner = new OMPTraitExprGroup();
  Inner->setGroupType(OMPTraitExprGroup::AND);
  Inner->addExpr(new OMPLiteralTrait(1));
  Inner->addExpr(new OMPWildcardTrait());

  Outer->addExpr(Inner);
  Outer->addExpr(new OMPLiteralTrait(2));

  // Mock: 4 devices
  Outer->setNumDevices([]() { return 4; });

  // Inner matches device 1 (literal 1 AND wildcard)
  // Outer matches 1 OR 2
  EXPECT_TRUE(Outer->match(1));
  EXPECT_TRUE(Outer->match(2));
  EXPECT_FALSE(Outer->match(0));
  EXPECT_FALSE(Outer->match(3));

  delete Outer;
}

//===----------------------------------------------------------------------===//
// OMPTraitClause Tests
//===----------------------------------------------------------------------===//

TEST(OMPTraitClauseTest, CreateAndDestroy) {
  OMPTraitClause *Clause = new OMPTraitClause();
  EXPECT_NE(Clause, nullptr);
  delete Clause;
}

TEST(OMPTraitClauseTest, SetExprWithTrait) {
  OMPTraitClause *Clause = new OMPTraitClause();
  Clause->setExpr(new OMPLiteralTrait(2));

  // The trait is wrapped in OMPTraitExprSingle internally
  OMPTraitExpr *Expr = Clause->getExpr();
  EXPECT_NE(Expr, nullptr);

  delete Clause;
}

TEST(OMPTraitClauseTest, SetExprWithExpr) {
  OMPTraitClause *Clause = new OMPTraitClause();
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();
  Group->addExpr(new OMPLiteralTrait(1));
  Clause->setExpr(Group);

  EXPECT_EQ(Clause->getExpr(), Group);

  delete Clause;
}

TEST(OMPTraitClauseTest, Equality) {
  OMPTraitClause *C1 = new OMPTraitClause();
  OMPTraitClause *C2 = new OMPTraitClause();

  C1->setExpr(new OMPLiteralTrait(1));
  C2->setExpr(new OMPLiteralTrait(1));

  EXPECT_TRUE(*C1 == *C2);

  delete C1;
  delete C2;
}

TEST(OMPTraitClauseTest, EqualityDifferentExprs) {
  OMPTraitClause *C1 = new OMPTraitClause();
  OMPTraitClause *C2 = new OMPTraitClause();

  C1->setExpr(new OMPLiteralTrait(1));
  C2->setExpr(new OMPLiteralTrait(2));

  EXPECT_FALSE(*C1 == *C2);

  delete C1;
  delete C2;
}

//===----------------------------------------------------------------------===//
// OMPTraitContext Tests
//===----------------------------------------------------------------------===//

TEST(OMPTraitContextTest, CreateAndDestroy) {
  OMPTraitContext *Context = new OMPTraitContext();
  EXPECT_NE(Context, nullptr);
  delete Context;
}

TEST(OMPTraitContextTest, AddClause) {
  OMPTraitContext *Context = new OMPTraitContext();
  OMPTraitClause *Clause = new OMPTraitClause();
  Clause->setExpr(new OMPLiteralTrait(2));
  Context->addClause(Clause);

  // Mock: 4 devices
  Context->setNumDevices([]() { return 4; });

  EXPECT_TRUE(Context->match(2));
  EXPECT_FALSE(Context->match(0));
  // Out of range
  EXPECT_FALSE(Context->match(5));

  delete Context;
}

TEST(OMPTraitContextTest, MultipleClauses) {
  OMPTraitContext *Context = new OMPTraitContext();

  OMPTraitClause *C1 = new OMPTraitClause();
  C1->setExpr(new OMPLiteralTrait(1));
  Context->addClause(C1);

  OMPTraitClause *C2 = new OMPTraitClause();
  C2->setExpr(new OMPLiteralTrait(2));
  Context->addClause(C2);

  OMPTraitClause *C3 = new OMPTraitClause();
  C3->setExpr(new OMPLiteralTrait(3));
  Context->addClause(C3);

  // Mock: 5 devices
  Context->setNumDevices([]() { return 5; });

  // Context uses OR semantics between clauses
  EXPECT_TRUE(Context->match(1));
  EXPECT_TRUE(Context->match(2));
  EXPECT_TRUE(Context->match(3));
  EXPECT_FALSE(Context->match(0));
  EXPECT_FALSE(Context->match(4));

  delete Context;
}

TEST(OMPTraitContextTest, EmptyContextMatchesNothing) {
  OMPTraitContext *Context = new OMPTraitContext();

  // Mock: 4 devices
  Context->setNumDevices([]() { return 4; });

  EXPECT_FALSE(Context->match(0));
  EXPECT_FALSE(Context->match(1));

  delete Context;
}

TEST(OMPTraitContextTest, WildcardClause) {
  OMPTraitContext *Context = new OMPTraitContext();
  OMPTraitClause *Clause = new OMPTraitClause();
  Clause->setExpr(new OMPWildcardTrait());
  Context->addClause(Clause);

  // Mock: 4 devices
  Context->setNumDevices([]() { return 4; });

  // In-range devices match
  EXPECT_TRUE(Context->match(0));
  EXPECT_TRUE(Context->match(3));
  // Out of range devices return false
  EXPECT_FALSE(Context->match(100));
  EXPECT_FALSE(Context->match(-1));

  delete Context;
}

TEST(OMPTraitContextTest, EvaluateWithMock) {
  OMPTraitContext *Context = new OMPTraitContext();

  // Mock: 5 devices
  Context->setNumDevices([]() { return 5; });

  OMPTraitClause *C1 = new OMPTraitClause();
  C1->setExpr(new OMPLiteralTrait(1));
  Context->addClause(C1);

  OMPTraitClause *C2 = new OMPTraitClause();
  C2->setExpr(new OMPLiteralTrait(3));
  Context->addClause(C2);

  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);
  EXPECT_TRUE(Result.contains(1));
  EXPECT_TRUE(Result.contains(3));
  EXPECT_FALSE(Result.contains(0));
  EXPECT_FALSE(Result.contains(2));
  EXPECT_FALSE(Result.contains(4));

  delete Context;
}

TEST(OMPTraitContextTest, Equality) {
  OMPTraitContext *Ctx1 = new OMPTraitContext();
  OMPTraitContext *Ctx2 = new OMPTraitContext();

  OMPTraitClause *C1 = new OMPTraitClause();
  C1->setExpr(new OMPLiteralTrait(1));
  Ctx1->addClause(C1);

  OMPTraitClause *C2 = new OMPTraitClause();
  C2->setExpr(new OMPLiteralTrait(1));
  Ctx2->addClause(C2);

  EXPECT_TRUE(*Ctx1 == *Ctx2);

  delete Ctx1;
  delete Ctx2;
}

TEST(OMPTraitContextTest, EqualityDifferentClauses) {
  OMPTraitContext *Ctx1 = new OMPTraitContext();
  OMPTraitContext *Ctx2 = new OMPTraitContext();

  OMPTraitClause *C1 = new OMPTraitClause();
  C1->setExpr(new OMPLiteralTrait(1));
  Ctx1->addClause(C1);

  OMPTraitClause *C2 = new OMPTraitClause();
  C2->setExpr(new OMPLiteralTrait(2));
  Ctx2->addClause(C2);

  EXPECT_FALSE(*Ctx1 == *Ctx2);

  delete Ctx1;
  delete Ctx2;
}

} // namespace
