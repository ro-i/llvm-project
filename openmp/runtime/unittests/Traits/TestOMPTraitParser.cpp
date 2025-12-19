//===- TestOMPTraitParser.cpp - Tests for OMP Trait Parser ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "kmp_traits.h"
#include "gtest/gtest.h"

namespace {

//===----------------------------------------------------------------------===//
// Helper to parse and auto-cleanup
//===----------------------------------------------------------------------===//

class ParserTest : public ::testing::Test {
protected:
  OMPTraitContext *Context = nullptr;

  void parse(const char *Spec) {
    Context = OMPTraitContext::parseFromSpec(StringRef(Spec));
  }

  void TearDown() override {
    if (Context) {
      delete Context;
      Context = nullptr;
    }
  }
};

template <bool ExpectedResult>
static void checkResultSingle(const OMPTraitContext *Context,
                              const Vector<int> &Result,
                              int ExpectedDeviceNum) {
  EXPECT_EQ(Context->match(ExpectedDeviceNum), ExpectedResult);
  EXPECT_EQ(Result.contains(ExpectedDeviceNum), ExpectedResult);
}

template <bool ExpectedResult, int... DeviceNums>
static void checkResult(const OMPTraitContext *Context,
                        const Vector<int> &Result) {
  (checkResultSingle<ExpectedResult>(Context, Result, DeviceNums), ...);
}

template <bool ExpectedResult, int... DeviceNums>
static void checkResult(const OMPTraitContext *Context) {
  Vector<int> Result = Context->evaluate();
  checkResult<ExpectedResult, DeviceNums...>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Literal Device Numbers
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, SingleLiteral) {
  parse("5");

  ASSERT_NE(Context, nullptr);
  // Device 5 is out of range (mock has 4 devices: 0-3), so match returns false
  EXPECT_EQ(Context->evaluate().size(), 0u);

  checkResult<false, 5, 0, 4, 6>(Context);
}

TEST_F(ParserTest, ZeroLiteral) {
  parse("0");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, MultipleLiterals) {
  parse("1,2,3");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 1, 2, 3>(Context, Result);
  checkResult<false, 0, 4>(Context, Result);
}

TEST_F(ParserTest, LiteralsWithSpaces) {
  parse("1, 2, 3");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 1, 2, 3>(Context, Result);
  checkResult<false, 0, 4>(Context, Result);
}

TEST_F(ParserTest, LiteralsWithLeadingSpaces) {
  parse("  1,  2,  3");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 1, 2, 3>(Context, Result);
  checkResult<false, 0, 4>(Context, Result);
}

TEST_F(ParserTest, LargeLiteral) {
  parse("12345");

  ASSERT_NE(Context, nullptr);
  // Device 12345 is out of range, so match returns false
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 0u);

  checkResult<false, 12345, 0>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Wildcard
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, Wildcard) {
  parse("*");

  ASSERT_NE(Context, nullptr);
  // Wildcard matches all 4 mock devices
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
  checkResult<false, 100>(Context, Result);
}

TEST_F(ParserTest, WildcardWithLiterals) {
  parse("1, *, 3");

  ASSERT_NE(Context, nullptr);
  // Wildcard makes all in-range devices match
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
  checkResult<false, 100>(Context, Result);
}

//===----------------------------------------------------------------------===//
// UID Traits
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, UIDTrait) {
  parse("uid(device-0)");

  ASSERT_NE(Context, nullptr);
  // Uses mock: device-0 is at index 0
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, UIDTraitWithUnderscore) {
  parse("uid(my_device_123)");

  ASSERT_NE(Context, nullptr);
  // This UID doesn't match any mock device
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 0u);

  checkResult<false, 0, 1>(Context, Result);
}

TEST_F(ParserTest, UIDTraitWithDash) {
  parse("uid(device-2)");

  ASSERT_NE(Context, nullptr);
  // Uses mock: device-2 is at index 2
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 2>(Context, Result);
  checkResult<false, 0, 1, 3>(Context, Result);
}

TEST_F(ParserTest, MultipleUIDTraits) {
  parse("uid(device-1), uid(device-3)");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 1, 3>(Context, Result);
  checkResult<false, 0, 2>(Context, Result);
}

TEST_F(ParserTest, MixedLiteralsAndUIDs) {
  parse("0, uid(device-2), 1, uid(device-3)");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Negation
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, NegatedUID) {
  parse("!uid(device-0)");

  ASSERT_NE(Context, nullptr);
  // Negated: everything except device-0 matches
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<false, 0>(Context, Result);
  checkResult<true, 1, 2, 3>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Grouping with Parentheses
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, SimpleGroup) {
  parse("(uid(device-1))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<false, 0>(Context, Result);
  checkResult<true, 1>(Context, Result);
}

TEST_F(ParserTest, GroupWithOR) {
  parse("(uid(device-0) || uid(device-2))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 2>(Context, Result);
  checkResult<false, 1, 3>(Context, Result);
}

TEST_F(ParserTest, GroupWithAND) {
  parse("(uid(device-0) && uid(device-0))");

  ASSERT_NE(Context, nullptr);
  // Both refer to same device, so AND passes for device 0
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, NegatedGroup) {
  parse("!(uid(device-0) || uid(device-1))");

  ASSERT_NE(Context, nullptr);
  // Negated: matches devices NOT in {0, 1}
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<false, 0, 1>(Context, Result);
  checkResult<true, 2, 3>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Complex Expressions
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, ComplexMixed) {
  parse("0, 1, uid(device-2), *");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
  checkResult<false, 100>(Context, Result);
}

TEST_F(ParserTest, MultipleORGroups) {
  parse("(uid(device-0) || uid(device-1)), (uid(device-2) || uid(device-3))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Complex Boolean Operators
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, ThreeWayOR) {
  // Three UIDs combined with OR
  parse("(uid(device-0) || uid(device-1) || uid(device-2))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 0, 1, 2>(Context, Result);
  checkResult<false, 3>(Context, Result);
}

TEST_F(ParserTest, FourWayOR) {
  // All four mock devices via OR
  parse("(uid(device-0) || uid(device-1) || uid(device-2) || uid(device-3))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ThreeWayAND) {
  // Three identical UIDs with AND - all must match same device
  parse("(uid(device-1) && uid(device-1) && uid(device-1))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 1>(Context, Result);
  checkResult<false, 0, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ANDWithDifferentUIDs) {
  // AND with different UIDs - can never match (device can't have two UIDs)
  parse("(uid(device-0) && uid(device-1))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 0u);

  checkResult<false, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedThreeWayOR) {
  // Negate a group of three UIDs - matches devices NOT in {0, 1, 2}
  parse("!(uid(device-0) || uid(device-1) || uid(device-2))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<false, 0, 1, 2>(Context, Result);
  checkResult<true, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedAND) {
  // Negate an AND group - since AND never matches, negation matches all
  parse("!(uid(device-0) && uid(device-1))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedANDWithSameUID) {
  // Negate an AND that matches device-0 - matches everything except 0
  parse("!(uid(device-0) && uid(device-0))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<false, 0>(Context, Result);
  checkResult<true, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NestedParensWithOR) {
  // Nested parentheses around OR
  parse("((uid(device-0) || uid(device-1)))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 1>(Context, Result);
  checkResult<false, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NestedParensWithAND) {
  // Nested parentheses around AND
  parse("((uid(device-2) && uid(device-2)))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 2>(Context, Result);
  checkResult<false, 0, 1, 3>(Context, Result);
}

TEST_F(ParserTest, DoubleNegation) {
  // Double negation: !!uid(device-0) should match device-0
  parse("!(!uid(device-0))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedNestedOR) {
  // Negate nested OR group
  parse("!((uid(device-0) || uid(device-1)))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<false, 0, 1>(Context, Result);
  checkResult<true, 2, 3>(Context, Result);
}

TEST_F(ParserTest, MultipleNegatedExprs) {
  // Multiple negated clauses - OR semantics between clauses
  parse("!uid(device-0), !uid(device-1)");

  ASSERT_NE(Context, nullptr);
  // First clause matches 1,2,3; Second clause matches 0,2,3
  // OR between clauses: union = all devices
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, MixedNegatedAndNonNegated) {
  // Mix negated and non-negated clauses
  parse("uid(device-0), !uid(device-0)");

  ASSERT_NE(Context, nullptr);
  // First matches 0, second matches 1,2,3 -> union = all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ComplexORGroupsInSeparateExprs) {
  // Complex OR groups as separate clauses
  parse("(uid(device-0) || uid(device-1)), (uid(device-2) || uid(device-3))");

  ASSERT_NE(Context, nullptr);
  // First matches 0,1; Second matches 2,3 -> union = all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedORGroupWithLiteral) {
  // Negated OR group combined with literal in separate clauses
  parse("!(uid(device-0) || uid(device-1)), 0");

  ASSERT_NE(Context, nullptr);
  // First matches 2,3; Second matches 0 -> union = 0,2,3
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 0, 2, 3>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, DeeplyNestedWithOperators) {
  // Deeply nested with operators
  parse("(((uid(device-0) || uid(device-1))))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 1>(Context, Result);
  checkResult<false, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ORWithSpacesAroundOperators) {
  // OR with lots of whitespace
  parse("(  uid(device-0)   ||   uid(device-2)   ||   uid(device-3)  )");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 0, 2, 3>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, ANDWithSpacesAroundOperators) {
  // AND with lots of whitespace
  parse("(  uid(device-1)   &&   uid(device-1)  )");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 1>(Context, Result);
  checkResult<false, 0, 2, 3>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Mixed && and || (in separate clauses/groups)
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, ORExprAndANDExpr) {
  // OR group in first clause, AND group in second clause
  parse("(uid(device-0) || uid(device-1)), (uid(device-2) && uid(device-2))");

  ASSERT_NE(Context, nullptr);
  // First clause matches 0,1; Second clause matches 2 -> union = 0,1,2
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 0, 1, 2>(Context, Result);
  checkResult<false, 3>(Context, Result);
}

TEST_F(ParserTest, ANDExprAndORExpr) {
  // AND group first, OR group second
  parse("(uid(device-0) && uid(device-0)), (uid(device-2) || uid(device-3))");

  ASSERT_NE(Context, nullptr);
  // First clause matches 0; Second clause matches 2,3 -> union = 0,2,3
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 0, 2, 3>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, MultipleANDAndORExprs) {
  // Multiple clauses alternating between AND and OR
  parse("(uid(device-0) && uid(device-0)), (uid(device-1) || uid(device-2)), "
        "(uid(device-3) && uid(device-3))");

  ASSERT_NE(Context, nullptr);
  // Expr 1 matches 0; Expr 2 matches 1,2; Expr 3 matches 3 -> all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedORWithAND) {
  // Negated OR clause combined with AND clause
  parse("!(uid(device-0) || uid(device-1)), (uid(device-0) && uid(device-0))");

  ASSERT_NE(Context, nullptr);
  // First clause matches 2,3; Second clause matches 0 -> union = 0,2,3
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 0, 2, 3>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, NegatedANDWithOR) {
  // Negated AND clause combined with OR clause
  parse("!(uid(device-0) && uid(device-0)), (uid(device-0) || uid(device-1))");

  ASSERT_NE(Context, nullptr);
  // First clause matches 1,2,3; Second clause matches 0,1 -> all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ComplexMixedOperators) {
  // Complex mix: OR, AND, negated OR, literal
  parse("(uid(device-0) || uid(device-1)), (uid(device-2) && uid(device-2)), "
        "!(uid(device-0) || uid(device-1) || uid(device-2)), 0");

  ASSERT_NE(Context, nullptr);
  // Expr 1: 0,1; Expr 2: 2; Expr 3: NOT(0,1,2) = 3; Expr 4: 0
  // Union = all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ANDNeverMatchesWithOR) {
  // AND that never matches combined with OR that does
  parse("(uid(device-0) && uid(device-1)), (uid(device-2) || uid(device-3))");

  ASSERT_NE(Context, nullptr);
  // First clause: never matches (different UIDs); Second: 2,3
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<false, 0, 1>(Context, Result);
  checkResult<true, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ORNeverMatchesWithAND) {
  // OR with non-existent UIDs combined with AND that matches
  parse("(uid(nonexistent-a) || uid(nonexistent-b)), (uid(device-0) && "
        "uid(device-0))");

  ASSERT_NE(Context, nullptr);
  // First clause: no match; Second: 0
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ThreeWayORAndThreeWayAND) {
  // Three-way OR and three-way AND in separate clauses
  parse("(uid(device-0) || uid(device-1) || uid(device-2)), (uid(device-3) && "
        "uid(device-3) && uid(device-3))");

  ASSERT_NE(Context, nullptr);
  // First: 0,1,2; Second: 3 -> all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedMixedExprs) {
  // Both clauses negated with different operators
  parse("!(uid(device-0) || uid(device-1)), !(uid(device-2) && uid(device-2))");

  ASSERT_NE(Context, nullptr);
  // First: NOT(0,1) = 2,3; Second: NOT(2) = 0,1,3
  // Union = all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, LiteralsWithMixedOperatorExprs) {
  // Literals combined with both OR and AND clauses
  parse("0, (uid(device-1) || uid(device-2)), 3, (uid(device-0) && "
        "uid(device-0))");

  ASSERT_NE(Context, nullptr);
  // Literals: 0,3; OR clause: 1,2; AND clause: 0
  // Union = all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Nested Mixed Operators (|| and && at different nesting levels)
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, ORContainingANDGroup) {
  // Outer OR with inner AND group: (A || (B && C))
  // For (B && C) to match, both B and C must match same device
  parse("(uid(device-0) || (uid(device-1) && uid(device-1)))");

  ASSERT_NE(Context, nullptr);
  // device-0 matches via first operand
  // device-1 matches via (device-1 && device-1)
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 1>(Context, Result);
  checkResult<false, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ANDContainingORGroup) {
  // Outer AND with inner OR group: (A && (B || C))
  // Both the trait A and the group (B || C) must match
  // Since A is uid(device-0), only device-0 can satisfy A
  // (B || C) must also match device-0 for AND to succeed
  parse("(uid(device-0) && (uid(device-0) || uid(device-1)))");

  ASSERT_NE(Context, nullptr);
  // device-0: uid(device-0) matches AND (uid(device-0) || uid(device-1))
  // matches -> true device-1: uid(device-0) doesn't match -> false
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ORWithTwoANDGroups) {
  // ((A && B) || (C && D)) - OR of two AND groups
  parse(
      "((uid(device-0) && uid(device-0)) || (uid(device-2) && uid(device-2)))");

  ASSERT_NE(Context, nullptr);
  // First AND matches device-0; Second AND matches device-2
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 2>(Context, Result);
  checkResult<false, 1, 3>(Context, Result);
}

TEST_F(ParserTest, ANDWithTwoORGroups) {
  // ((A || B) && (C || D)) - AND of two OR groups
  // For a device to match: must match (A || B) AND must match (C || D)
  parse(
      "((uid(device-0) || uid(device-1)) && (uid(device-0) || uid(device-2)))");

  ASSERT_NE(Context, nullptr);
  // device-0: matches (0||1) AND matches (0||2) -> true
  // device-1: matches (0||1) but NOT (0||2) -> false
  // device-2: NOT (0||1) -> false
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ORWithNestedANDContainingOR) {
  // (A || (B && (C || D))) - three levels of nesting
  parse(
      "(uid(device-3) || (uid(device-0) && (uid(device-0) || uid(device-1))))");

  ASSERT_NE(Context, nullptr);
  // device-0: inner (0||1) matches, uid(device-0) matches -> AND matches; OR
  // satisfied device-1: inner (0||1) matches, but uid(device-0) doesn't -> AND
  // fails; outer uid(device-3) fails device-3: outer uid(device-3) matches
  // directly
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 3>(Context, Result);
  checkResult<false, 1, 2>(Context, Result);
}

TEST_F(ParserTest, ANDWithNestedORContainingAND) {
  // (A && (B || (C && D))) - three levels of nesting
  parse(
      "(uid(device-0) && (uid(device-0) || (uid(device-1) && uid(device-1))))");

  ASSERT_NE(Context, nullptr);
  // device-0: uid(device-0) matches; inner (uid(device-0) || ...) matches ->
  // AND satisfied device-1: uid(device-0) doesn't match -> AND fails
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedORContainingAND) {
  // !(A || (B && C)) - negated complex expression
  parse("!(uid(device-0) || (uid(device-1) && uid(device-1)))");

  ASSERT_NE(Context, nullptr);
  // Without negation: matches 0, 1
  // With negation: matches 2, 3
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<false, 0, 1>(Context, Result);
  checkResult<true, 2, 3>(Context, Result);
}

TEST_F(ParserTest, NegatedANDContainingOR) {
  // !(A && (B || C)) - negated complex expression
  parse("!(uid(device-0) && (uid(device-0) || uid(device-1)))");

  ASSERT_NE(Context, nullptr);
  // Without negation: matches only 0
  // With negation: matches 1, 2, 3
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<false, 0>(Context, Result);
  checkResult<true, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ComplexNestedWithAllDevices) {
  // ((A || B) && (C || D)) where union covers all but AND restricts
  parse(
      "((uid(device-0) || uid(device-1)) && (uid(device-1) || uid(device-2)))");

  ASSERT_NE(Context, nullptr);
  // device-0: (0||1)=true, (1||2)=false -> AND=false
  // device-1: (0||1)=true, (1||2)=true -> AND=true
  // device-2: (0||1)=false -> AND=false
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 1>(Context, Result);
  checkResult<false, 0, 2, 3>(Context, Result);
}

TEST_F(ParserTest, TripleNestedMixedOperators) {
  // (((A || B) && C) || D) - deeply nested with alternating operators
  parse(
      "(((uid(device-0) || uid(device-1)) && uid(device-0)) || uid(device-3))");

  ASSERT_NE(Context, nullptr);
  // Inner (0||1): matches 0, 1
  // Middle ((0||1) && 0): matches only 0
  // Outer (... || 3): matches 0, 3
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 3>(Context, Result);
  checkResult<false, 1, 2>(Context, Result);
}

TEST_F(ParserTest, ANDChainWithNestedOR) {
  // (A && (B || C) && D) - wait, this mixes operators at same level
  // Actually: ((A && (B || C)) is valid - let's do that
  // Let's do: (uid(device-0) && (uid(device-0) || uid(device-1)) &&
  // uid(device-0)) This is three-way AND where middle operand is an OR group
  parse("(uid(device-0) && (uid(device-0) || uid(device-1)) && uid(device-0))");

  ASSERT_NE(Context, nullptr);
  // All three must match: uid(device-0), (0||1), uid(device-0)
  // Only device-0 satisfies all
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1, 2, 3>(Context, Result);
}

TEST_F(ParserTest, ORChainWithNestedAND) {
  // (A || (B && C) || D) - three-way OR where middle is AND group
  parse("(uid(device-0) || (uid(device-1) && uid(device-1)) || uid(device-3))");

  ASSERT_NE(Context, nullptr);
  // Any of: device-0, (device-1 && device-1), device-3
  // Matches: 0, 1, 3
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 0, 1, 3>(Context, Result);
  checkResult<false, 2>(Context, Result);
}

TEST_F(ParserTest, NestedMixedWithSpaces) {
  // Nested mixed operators with lots of whitespace
  parse("(  uid(device-0)  ||  ( uid(device-1)  &&  uid(device-1) )  ||  "
        "uid(device-2)  )");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 0, 1, 2>(Context, Result);
  checkResult<false, 3>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Empty Input
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, EmptyString) {
  parse("");

  ASSERT_NE(Context, nullptr);
  // Empty context matches nothing
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 0u);

  checkResult<false, 0, 1>(Context, Result);
}

TEST_F(ParserTest, OnlyWhitespace) {
  parse("   ");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 0u);

  checkResult<false, 0>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Edge Cases
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, SingleDigitLiterals) {
  parse("0,1,2,3,4,5,6,7,8,9");

  ASSERT_NE(Context, nullptr);
  // Only devices 0-3 are in range
  // Devices 4-10 are out of range
  // Mock has only 4 devices (0-3)
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
  checkResult<false, 4, 5, 6, 7, 8, 9, 10>(Context, Result);
}

TEST_F(ParserTest, ConsecutiveCommas) {
  // This should fail or be handled gracefully
  // The parser requires content between commas
}

TEST_F(ParserTest, SpacesAroundOperators) {
  parse("( uid(device-0)  ||  uid(device-1) )");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 1>(Context, Result);
  checkResult<false, 2>(Context, Result);
}

TEST_F(ParserTest, NestedParens) {
  parse("((uid(device-0)))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, DeeplyNested) {
  parse("(((uid(device-2))))");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<false, 0, 1>(Context, Result);
  checkResult<true, 2>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Regression Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, NoSpaceAfterComma) {
  parse("1,2,3");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 1, 2, 3>(Context, Result);
  checkResult<false, 0, 4>(Context, Result);
}

TEST_F(ParserTest, SpaceBeforeComma) {
  parse("1 , 2 , 3");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<true, 1, 2, 3>(Context, Result);
  checkResult<false, 0, 4>(Context, Result);
}

TEST_F(ParserTest, MixedWildcardAndLiteral) {
  parse("*, 5");

  ASSERT_NE(Context, nullptr);
  // Wildcard matches all in-range devices
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
  checkResult<false, 4, 5, 6, 7, 8, 9, 10>(Context, Result);
}

//===----------------------------------------------------------------------===//
// Real-World Examples
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, SelectFirstDevice) {
  parse("0");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 0>(Context, Result);
  checkResult<false, 1>(Context, Result);
}

TEST_F(ParserTest, SelectAllDevices) {
  parse("*");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 4u);

  checkResult<true, 0, 1, 2, 3>(Context, Result);
  checkResult<false, 4, 5, 6, 7, 8, 9, 10>(Context, Result);
}

TEST_F(ParserTest, SelectSpecificDevices) {
  parse("0, 2, 4");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 2u);

  checkResult<true, 0, 2>(Context, Result);
  checkResult<false, 1, 3>(Context, Result);
}

TEST_F(ParserTest, SelectByUID) {
  parse("uid(device-1)");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 1u);

  checkResult<true, 1>(Context, Result);
  checkResult<false, 0, 2>(Context, Result);
}

TEST_F(ParserTest, ExcludeByUID) {
  parse("!uid(device-0)");

  ASSERT_NE(Context, nullptr);
  Vector<int> Result = Context->evaluate();
  EXPECT_EQ(Result.size(), 3u);

  checkResult<false, 0>(Context, Result);
  checkResult<true, 1, 2, 3>(Context, Result);
}

} // namespace
