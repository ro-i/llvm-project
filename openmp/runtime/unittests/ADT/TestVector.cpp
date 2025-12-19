//===- TestVector.cpp - Tests for Vector class ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "kmp_adt.h"
#include "gtest/gtest.h"

namespace {

//===----------------------------------------------------------------------===//
// Construction
//===----------------------------------------------------------------------===//

TEST(VectorTest, DefaultConstruction) {
  Vector<int> V;
  EXPECT_EQ(V.size(), 0u);
  EXPECT_TRUE(V.empty());
}

TEST(VectorTest, ConstructWithCapacity) {
  Vector<int> V(10);
  EXPECT_EQ(V.size(), 0u);
  EXPECT_TRUE(V.empty());
}

TEST(VectorTest, ConstructWithData) {
  int Data[] = {1, 2, 3, 4, 5};
  Vector<int> V(5, Data, 5);

  EXPECT_EQ(V.size(), 5u);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
  EXPECT_EQ(V[3], 4);
  EXPECT_EQ(V[4], 5);
}

TEST(VectorTest, ConstructWithCapacityLargerThanSize) {
  int Data[] = {1, 2, 3};
  Vector<int> V(10, Data, 3);

  EXPECT_EQ(V.size(), 3u);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
}

//===----------------------------------------------------------------------===//
// Copy Semantics
//===----------------------------------------------------------------------===//

TEST(VectorTest, CopyConstruction) {
  int Data[] = {1, 2, 3};
  Vector<int> V1(3, Data, 3);
  Vector<int> V2(V1);

  EXPECT_EQ(V2.size(), 3u);
  EXPECT_EQ(V2[0], 1);
  EXPECT_EQ(V2[1], 2);
  EXPECT_EQ(V2[2], 3);

  // Modify V1, V2 should be unchanged
  V1[0] = 100;
  EXPECT_EQ(V2[0], 1);
}

TEST(VectorTest, CopyAssignment) {
  int Data1[] = {1, 2, 3};
  int Data2[] = {4, 5};
  Vector<int> V1(3, Data1, 3);
  Vector<int> V2(2, Data2, 2);

  V2 = V1;

  EXPECT_EQ(V2.size(), 3u);
  EXPECT_EQ(V2[0], 1);
  EXPECT_EQ(V2[1], 2);
  EXPECT_EQ(V2[2], 3);
}

TEST(VectorTest, SelfCopyAssignment) {
  int Data[] = {1, 2, 3};
  Vector<int> V(3, Data, 3);

  Vector<int> &VRef = V;
  V = VRef; // Avoid self-assignment warning

  EXPECT_EQ(V.size(), 3u);
  EXPECT_EQ(V[0], 1);
}

//===----------------------------------------------------------------------===//
// Move Semantics
//===----------------------------------------------------------------------===//

TEST(VectorTest, MoveConstruction) {
  int Data[] = {1, 2, 3};
  Vector<int> V1(3, Data, 3);
  Vector<int> V2(std::move(V1));

  EXPECT_EQ(V2.size(), 3u);
  EXPECT_EQ(V2[0], 1);
  EXPECT_EQ(V2[1], 2);
  EXPECT_EQ(V2[2], 3);

  // V1 should be empty after move
  EXPECT_EQ(V1.size(), 0u);
}

TEST(VectorTest, MoveAssignment) {
  int Data1[] = {1, 2, 3};
  int Data2[] = {4, 5};
  Vector<int> V1(3, Data1, 3);
  Vector<int> V2(2, Data2, 2);

  V2 = std::move(V1);

  EXPECT_EQ(V2.size(), 3u);
  EXPECT_EQ(V2[0], 1);
  EXPECT_EQ(V1.size(), 0u);
}

TEST(VectorTest, SelfMoveAssignment) {
  int Data[] = {1, 2, 3};
  Vector<int> V(3, Data, 3);

  Vector<int> &VRef = V;
  V = std::move(VRef); // Avoid self-move warning

  // Self-move should leave object in valid state
  EXPECT_EQ(V.size(), 3u);
  EXPECT_EQ(V[0], 1);
}

//===----------------------------------------------------------------------===//
// pushBack
//===----------------------------------------------------------------------===//

TEST(VectorTest, PushBackToEmpty) {
  Vector<int> V;

  V.pushBack(42);

  EXPECT_EQ(V.size(), 1u);
  EXPECT_EQ(V[0], 42);
}

TEST(VectorTest, PushBackMultiple) {
  Vector<int> V;

  V.pushBack(1);
  V.pushBack(2);
  V.pushBack(3);
  V.pushBack(4);
  V.pushBack(5);

  EXPECT_EQ(V.size(), 5u);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
  EXPECT_EQ(V[3], 4);
  EXPECT_EQ(V[4], 5);
}

TEST(VectorTest, PushBackGrowth) {
  Vector<int> V;

  // Push many elements to trigger multiple resizes
  for (int I = 0; I < 100; ++I) {
    V.pushBack(I);
  }

  EXPECT_EQ(V.size(), 100u);
  for (int I = 0; I < 100; ++I) {
    EXPECT_EQ(V[I], I);
  }
}

//===----------------------------------------------------------------------===//
// clear
//===----------------------------------------------------------------------===//

TEST(VectorTest, Clear) {
  int Data[] = {1, 2, 3};
  Vector<int> V(3, Data, 3);

  EXPECT_EQ(V.size(), 3u);

  V.clear();

  EXPECT_EQ(V.size(), 0u);
  EXPECT_TRUE(V.empty());
}

TEST(VectorTest, ClearEmpty) {
  Vector<int> V;

  V.clear();

  EXPECT_EQ(V.size(), 0u);
  EXPECT_TRUE(V.empty());
}

TEST(VectorTest, PushBackAfterClear) {
  int Data[] = {1, 2, 3};
  Vector<int> V(3, Data, 3);

  V.clear();
  V.pushBack(42);

  EXPECT_EQ(V.size(), 1u);
  EXPECT_EQ(V[0], 42);
}

//===----------------------------------------------------------------------===//
// empty
//===----------------------------------------------------------------------===//

TEST(VectorTest, EmptyOnDefault) {
  Vector<int> V;
  EXPECT_TRUE(V.empty());
}

TEST(VectorTest, NotEmptyAfterPush) {
  Vector<int> V;
  V.pushBack(1);
  EXPECT_FALSE(V.empty());
}

TEST(VectorTest, EmptyAfterClear) {
  Vector<int> V;
  V.pushBack(1);
  V.clear();
  EXPECT_TRUE(V.empty());
}

//===----------------------------------------------------------------------===//
// contains
//===----------------------------------------------------------------------===//

TEST(VectorTest, ContainsFound) {
  int Data[] = {1, 2, 3, 4, 5};
  Vector<int> V(5, Data, 5);

  EXPECT_TRUE(V.contains(1));
  EXPECT_TRUE(V.contains(3));
  EXPECT_TRUE(V.contains(5));
}

TEST(VectorTest, ContainsNotFound) {
  int Data[] = {1, 2, 3, 4, 5};
  Vector<int> V(5, Data, 5);

  EXPECT_FALSE(V.contains(0));
  EXPECT_FALSE(V.contains(6));
  EXPECT_FALSE(V.contains(-1));
}

TEST(VectorTest, ContainsEmpty) {
  Vector<int> V;

  EXPECT_FALSE(V.contains(0));
  EXPECT_FALSE(V.contains(1));
}

//===----------------------------------------------------------------------===//
// isSetEqual
//===----------------------------------------------------------------------===//

TEST(VectorTest, IsSetEqualSameOrder) {
  int Data[] = {1, 2, 3};
  Vector<int> V1(3, Data, 3);
  Vector<int> V2(3, Data, 3);

  EXPECT_TRUE(V1.isSetEqual(V2));
  EXPECT_TRUE(V2.isSetEqual(V1));
}

TEST(VectorTest, IsSetEqualDifferentOrder) {
  int Data1[] = {1, 2, 3};
  int Data2[] = {3, 1, 2};
  Vector<int> V1(3, Data1, 3);
  Vector<int> V2(3, Data2, 3);

  EXPECT_TRUE(V1.isSetEqual(V2));
  EXPECT_TRUE(V2.isSetEqual(V1));
}

TEST(VectorTest, IsSetEqualDifferentSize) {
  int Data1[] = {1, 2, 3};
  int Data2[] = {1, 2};
  Vector<int> V1(3, Data1, 3);
  Vector<int> V2(2, Data2, 2);

  EXPECT_FALSE(V1.isSetEqual(V2));
  EXPECT_FALSE(V2.isSetEqual(V1));
}

TEST(VectorTest, IsSetEqualDifferentElements) {
  int Data1[] = {1, 2, 3};
  int Data2[] = {1, 2, 4};
  Vector<int> V1(3, Data1, 3);
  Vector<int> V2(3, Data2, 3);

  EXPECT_FALSE(V1.isSetEqual(V2));
}

TEST(VectorTest, IsSetEqualEmpty) {
  Vector<int> V1;
  Vector<int> V2;

  EXPECT_TRUE(V1.isSetEqual(V2));
}

//===----------------------------------------------------------------------===//
// contains with Comparator
//===----------------------------------------------------------------------===//

TEST(VectorTest, ContainsWithComparator) {
  int Data[] = {10, 20, 30};
  Vector<int> V(3, Data, 3);

  // Compare by tens digit
  auto SameTens = [](const int &A, const int &B) {
    return (A / 10) == (B / 10);
  };

  EXPECT_TRUE(V.contains(15, SameTens)); // 15/10 == 1, matches 10/10 == 1
  EXPECT_TRUE(V.contains(25, SameTens)); // 25/10 == 2, matches 20/10 == 2
  EXPECT_FALSE(V.contains(45, SameTens)); // 45/10 == 4, no match
}

TEST(VectorTest, ContainsPointerWithComparator) {
  int A = 100, B = 200, C = 300;
  int *Data[] = {&A, &B, &C};
  Vector<int *> V(3, Data, 3);

  // Comparator that compares pointed-to values
  auto DerefComp = [](int *const &PA, int *const &PB) { return *PA == *PB; };

  int X = 200;
  int *PX = &X;

  // Without comparator: comparing pointers (different addresses)
  EXPECT_FALSE(V.contains(PX));

  // With comparator: comparing values (200 == 200)
  EXPECT_TRUE(V.contains(PX, DerefComp));
}

//===----------------------------------------------------------------------===//
// isSetEqual with Comparator
//===----------------------------------------------------------------------===//

TEST(VectorTest, IsSetEqualWithComparator) {
  int Data1[] = {10, 20, 30};
  int Data2[] = {35, 15, 25}; // Same tens digits as Data1, different order
  Vector<int> V1(3, Data1, 3);
  Vector<int> V2(3, Data2, 3);

  auto SameTens = [](const int &A, const int &B) {
    return (A / 10) == (B / 10);
  };

  // Without comparator: not equal
  EXPECT_FALSE(V1.isSetEqual(V2));

  // With comparator: equal (same tens digits)
  EXPECT_TRUE(V1.isSetEqual(V2, SameTens));
}

TEST(VectorTest, IsSetEqualPointerWithComparator) {
  int A1 = 100, B1 = 200, C1 = 300;
  int A2 = 100, B2 = 200, C2 = 300;
  int *Data1[] = {&A1, &B1, &C1};
  int *Data2[] = {&C2, &A2, &B2}; // Same values, different order and addresses
  Vector<int *> V1(3, Data1, 3);
  Vector<int *> V2(3, Data2, 3);

  auto DerefComp = [](int *const &PA, int *const &PB) { return *PA == *PB; };

  // Without comparator: not equal (different pointers)
  EXPECT_FALSE(V1.isSetEqual(V2));

  // With comparator: equal (same pointed-to values)
  EXPECT_TRUE(V1.isSetEqual(V2, DerefComp));
}

//===----------------------------------------------------------------------===//
// Indexing
//===----------------------------------------------------------------------===//

TEST(VectorTest, IndexOperator) {
  int Data[] = {10, 20, 30};
  Vector<int> V(3, Data, 3);

  EXPECT_EQ(V[0], 10);
  EXPECT_EQ(V[1], 20);
  EXPECT_EQ(V[2], 30);
}

TEST(VectorTest, IndexOperatorModify) {
  int Data[] = {10, 20, 30};
  Vector<int> V(3, Data, 3);

  V[1] = 200;

  EXPECT_EQ(V[1], 200);
}

TEST(VectorTest, ConstIndexOperator) {
  int Data[] = {10, 20, 30};
  const Vector<int> V(3, Data, 3);

  EXPECT_EQ(V[0], 10);
  EXPECT_EQ(V[1], 20);
  EXPECT_EQ(V[2], 30);
}

//===----------------------------------------------------------------------===//
// Iterators
//===----------------------------------------------------------------------===//

TEST(VectorTest, BeginEnd) {
  int Data[] = {1, 2, 3};
  Vector<int> V(3, Data, 3);

  int *Begin = V.begin();
  int *End = V.end();

  EXPECT_EQ(End - Begin, 3);
  EXPECT_EQ(*Begin, 1);
  EXPECT_EQ(*(End - 1), 3);
}

TEST(VectorTest, ConstBeginEnd) {
  int Data[] = {1, 2, 3};
  const Vector<int> V(3, Data, 3);

  const int *Begin = V.begin();
  const int *End = V.end();

  EXPECT_EQ(End - Begin, 3);
  EXPECT_EQ(*Begin, 1);
}

TEST(VectorTest, RangeBasedFor) {
  int Data[] = {1, 2, 3, 4, 5};
  Vector<int> V(5, Data, 5);

  int Sum = 0;
  for (int X : V) {
    Sum += X;
  }

  EXPECT_EQ(Sum, 15);
}

TEST(VectorTest, RangeBasedForModify) {
  int Data[] = {1, 2, 3};
  Vector<int> V(3, Data, 3);

  for (int &X : V) {
    X *= 2;
  }

  EXPECT_EQ(V[0], 2);
  EXPECT_EQ(V[1], 4);
  EXPECT_EQ(V[2], 6);
}

TEST(VectorTest, RangeBasedForEmpty) {
  Vector<int> V;

  int Count = 0;
  for (int X : V) {
    (void)X;
    Count++;
  }

  EXPECT_EQ(Count, 0);
}

//===----------------------------------------------------------------------===//
// Edge Cases
//===----------------------------------------------------------------------===//

TEST(VectorTest, EmptyVector) {
  Vector<int> V;

  EXPECT_EQ(V.size(), 0u);
  EXPECT_EQ(V.begin(), V.end());
  EXPECT_FALSE(V.contains(0));
}

TEST(VectorTest, SingleElement) {
  Vector<int> V;
  V.pushBack(42);

  EXPECT_EQ(V.size(), 1u);
  EXPECT_EQ(V[0], 42);
  EXPECT_TRUE(V.contains(42));
  EXPECT_EQ(V.end() - V.begin(), 1);
}

//===----------------------------------------------------------------------===//
// Different Types
//===----------------------------------------------------------------------===//

TEST(VectorTest, PointerType) {
  int A = 1, B = 2, C = 3;
  int *Data[] = {&A, &B, &C};
  Vector<int *> V(3, Data, 3);

  EXPECT_EQ(V.size(), 3u);
  EXPECT_EQ(*V[0], 1);
  EXPECT_EQ(*V[1], 2);
  EXPECT_EQ(*V[2], 3);
}

TEST(VectorTest, SizeTType) {
  size_t Data[] = {100, 200, 300};
  Vector<size_t> V(3, Data, 3);

  EXPECT_EQ(V[0], 100u);
  EXPECT_EQ(V[1], 200u);
  EXPECT_EQ(V[2], 300u);
}

//===----------------------------------------------------------------------===//
// Reserve
//===----------------------------------------------------------------------===//

TEST(VectorTest, Reserve) {
  Vector<int> V;
  V.reserve(100);

  EXPECT_EQ(V.size(), 0u);

  // Should be able to push without reallocation
  for (int I = 0; I < 100; ++I) {
    V.pushBack(I);
  }

  EXPECT_EQ(V.size(), 100u);
}

} // namespace
