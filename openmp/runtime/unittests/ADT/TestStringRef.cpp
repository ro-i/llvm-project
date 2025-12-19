//===- TestStringRef.cpp - Tests for StringRef class ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "kmp_adt.h"
#include "kmp.h"
#include "gtest/gtest.h"
#include <cstring>

namespace {

// Helper to compare StringRef content with a C string
static bool equals(const StringRef &S, const char *Expected) {
  size_t ExpectedLen = strlen(Expected);
  if (S.length() != ExpectedLen)
    return false;
  return memcmp(S.begin(), Expected, ExpectedLen) == 0;
}

//===----------------------------------------------------------------------===//
// Construction and Basic Properties
//===----------------------------------------------------------------------===//

TEST(StringRefTest, ConstructFromCString) {
  StringRef S("Hello");
  EXPECT_EQ(S.length(), 5u);
  EXPECT_TRUE(equals(S, "Hello"));
}

TEST(StringRefTest, ConstructFromCStringWithLength) {
  StringRef S("Hello World", 5);
  EXPECT_EQ(S.length(), 5u);
  EXPECT_TRUE(equals(S, "Hello"));
}

TEST(StringRefTest, ConstructEmpty) {
  StringRef S("");
  EXPECT_EQ(S.length(), 0u);
  EXPECT_TRUE(S.empty());
}

TEST(StringRefTest, Length) {
  EXPECT_EQ(StringRef("").length(), 0u);
  EXPECT_EQ(StringRef("a").length(), 1u);
  EXPECT_EQ(StringRef("hello").length(), 5u);
  EXPECT_EQ(StringRef("hello world").length(), 11u);
}

//===----------------------------------------------------------------------===//
// empty
//===----------------------------------------------------------------------===//

TEST(StringRefTest, EmptyString) {
  StringRef S("");
  EXPECT_TRUE(S.empty());
}

TEST(StringRefTest, NonEmptyString) {
  StringRef S("hello");
  EXPECT_FALSE(S.empty());
}

TEST(StringRefTest, EmptyAfterConsumeFront) {
  StringRef S("hello");
  EXPECT_FALSE(S.empty());

  S.consumeFront("hello");

  EXPECT_TRUE(S.empty());
  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, EmptyAfterDropFront) {
  StringRef S("abc");
  EXPECT_FALSE(S.empty());

  S.dropFront(3);

  EXPECT_TRUE(S.empty());
  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, EmptyAfterDropWhile) {
  StringRef S("12345");
  EXPECT_FALSE(S.empty());

  S.dropWhile([](char C) {
    return static_cast<bool>(isdigit(static_cast<unsigned char>(C)));
  });

  EXPECT_TRUE(S.empty());
  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, EmptyAfterConsumeInteger) {
  StringRef S("42");
  int Value = 0;
  EXPECT_FALSE(S.empty());

  S.consumeInteger(Value);

  EXPECT_TRUE(S.empty());
  EXPECT_EQ(S.length(), 0u);
  EXPECT_EQ(Value, 42);
}

TEST(StringRefTest, NotEmptyAfterPartialConsume) {
  StringRef S("123abc");
  int Value = 0;

  S.consumeInteger(Value);

  EXPECT_FALSE(S.empty());
  EXPECT_EQ(S.length(), 3u);
  EXPECT_TRUE(equals(S, "abc"));
}

//===----------------------------------------------------------------------===//
// Iterators
//===----------------------------------------------------------------------===//

TEST(StringRefTest, BeginEnd) {
  StringRef S("Hello");
  EXPECT_EQ(S.end() - S.begin(), 5);
  EXPECT_EQ(*S.begin(), 'H');
}

TEST(StringRefTest, RangeBasedFor) {
  StringRef S("abc");
  std::string Result;
  for (char C : S) {
    Result += C;
  }
  EXPECT_EQ(Result, "abc");
}

//===----------------------------------------------------------------------===//
// Assignment
//===----------------------------------------------------------------------===//

TEST(StringRefTest, Assignment) {
  StringRef S1("First");
  StringRef S2("Second");

  S1 = S2;

  EXPECT_TRUE(equals(S1, "Second"));
  EXPECT_EQ(S1.length(), 6u);
}

TEST(StringRefTest, SelfAssignment) {
  StringRef S("Test");
  StringRef &SRef = S;
  S = SRef; // Avoid self-assignment warning
  EXPECT_TRUE(equals(S, "Test"));
  EXPECT_EQ(S.length(), 4u);
}

//===----------------------------------------------------------------------===//
// consumeFront
//===----------------------------------------------------------------------===//

TEST(StringRefTest, ConsumeFrontSuccess) {
  StringRef S("Hello World");

  EXPECT_TRUE(S.consumeFront("Hello"));
  EXPECT_EQ(S.length(), 6u);
  EXPECT_TRUE(equals(S, " World"));
}

TEST(StringRefTest, ConsumeFrontFailure) {
  StringRef S("Hello World");

  EXPECT_FALSE(S.consumeFront("World"));
  EXPECT_EQ(S.length(), 11u);
  EXPECT_TRUE(equals(S, "Hello World"));
}

TEST(StringRefTest, ConsumeFrontEmpty) {
  StringRef S("Hello");

  EXPECT_TRUE(S.consumeFront(""));
  EXPECT_EQ(S.length(), 5u);
}

TEST(StringRefTest, ConsumeFrontTooLong) {
  StringRef S("Hi");

  EXPECT_FALSE(S.consumeFront("Hello"));
  EXPECT_EQ(S.length(), 2u);
}

TEST(StringRefTest, ConsumeFrontExact) {
  StringRef S("Hello");

  EXPECT_TRUE(S.consumeFront("Hello"));
  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, ConsumeFrontMultiple) {
  StringRef S("prefix:middle:suffix");

  EXPECT_TRUE(S.consumeFront("prefix"));
  EXPECT_TRUE(S.consumeFront(":"));
  EXPECT_TRUE(S.consumeFront("middle"));
  EXPECT_TRUE(S.consumeFront(":"));
  EXPECT_TRUE(equals(S, "suffix"));
}

//===----------------------------------------------------------------------===//
// consumeInteger
//===----------------------------------------------------------------------===//

TEST(StringRefTest, ConsumeIntegerSimple) {
  StringRef S("42");
  int Value = 0;

  EXPECT_TRUE(S.consumeInteger(Value));
  EXPECT_EQ(Value, 42);
  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, ConsumeIntegerWithTrailing) {
  StringRef S("123abc");
  int Value = 0;

  EXPECT_TRUE(S.consumeInteger(Value));
  EXPECT_EQ(Value, 123);
  EXPECT_TRUE(equals(S, "abc"));
}

TEST(StringRefTest, ConsumeIntegerZero) {
  StringRef S("0");
  int Value = -1;

  // AllowZero = true by default
  EXPECT_TRUE(S.consumeInteger(Value));
  EXPECT_EQ(Value, 0);
  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, ConsumeIntegerZeroNotAllowed) {
  StringRef S("0rest");
  int Value = -1;

  EXPECT_FALSE(S.consumeInteger(Value, /*AllowZero=*/false));
  // State should be restored on failure
  EXPECT_TRUE(equals(S, "0rest"));
}

TEST(StringRefTest, ConsumeIntegerNoDigits) {
  StringRef S("abc");
  int Value = -1;

  // No digits to consume, should fail
  EXPECT_FALSE(S.consumeInteger(Value));
  // String should be unchanged
  EXPECT_TRUE(equals(S, "abc"));
}

TEST(StringRefTest, ConsumeIntegerEmpty) {
  StringRef S("");
  int Value = -1;

  // Empty string has no digits, should fail
  EXPECT_FALSE(S.consumeInteger(Value));
}

TEST(StringRefTest, ConsumeIntegerLeadingZero) {
  StringRef S("007");
  int Value = -1;

  EXPECT_TRUE(S.consumeInteger(Value));
  EXPECT_EQ(Value, 7);
  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, ConsumeIntegerNegativeAllowed) {
  StringRef S("-42rest");
  int Value = 0;

  EXPECT_TRUE(S.consumeInteger(Value, true, true));
  EXPECT_EQ(Value, -42);
  EXPECT_TRUE(equals(S, "rest"));
}

TEST(StringRefTest, ConsumeIntegerNegativeNotAllowed) {
  StringRef S("-42");
  int Value = 0;

  EXPECT_FALSE(S.consumeInteger(Value, true, false));
  // State should be restored on failure
  EXPECT_TRUE(equals(S, "-42"));
}

TEST(StringRefTest, ConsumeIntegerMultipleDigits) {
  StringRef S("1234567890");
  int Value = 0;

  EXPECT_TRUE(S.consumeInteger(Value));
  EXPECT_EQ(Value, 1234567890);
}

//===----------------------------------------------------------------------===//
// copy
//===----------------------------------------------------------------------===//

TEST(StringRefTest, Copy) {
  StringRef S("Hello");
  char *Copied = S.copy();

  EXPECT_NE(Copied, nullptr);
  EXPECT_STREQ(Copied, "Hello");
  EXPECT_NE(Copied, S.begin()); // Different pointer

  KMP_INTERNAL_FREE(Copied);
}

TEST(StringRefTest, CopyEmpty) {
  StringRef S("");
  char *Copied = S.copy();

  EXPECT_NE(Copied, nullptr);
  EXPECT_STREQ(Copied, "");

  KMP_INTERNAL_FREE(Copied);
}

TEST(StringRefTest, CopySubstring) {
  // Test copying a substring that doesn't have a null terminator at Len
  StringRef Full("device-0)rest");
  StringRef Sub = Full.takeWhile([](char C) { return C != ')'; });

  EXPECT_EQ(Sub.length(), 8u); // "device-0"

  char *Copied = Sub.copy();

  EXPECT_NE(Copied, nullptr);
  EXPECT_STREQ(Copied, "device-0"); // Should NOT include ")"
  EXPECT_EQ(strlen(Copied), 8u);

  KMP_INTERNAL_FREE(Copied);
}

//===----------------------------------------------------------------------===//
// dropFront
//===----------------------------------------------------------------------===//

TEST(StringRefTest, DropFront) {
  StringRef S("Hello World");

  S.dropFront(6);

  EXPECT_EQ(S.length(), 5u);
  EXPECT_TRUE(equals(S, "World"));
}

TEST(StringRefTest, DropFrontZero) {
  StringRef S("Hello");

  S.dropFront(0);

  EXPECT_EQ(S.length(), 5u);
  EXPECT_TRUE(equals(S, "Hello"));
}

TEST(StringRefTest, DropFrontAll) {
  StringRef S("Hello");

  S.dropFront(5);

  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, DropFrontMoreThanLength) {
  StringRef S("Hi");

  S.dropFront(100);

  EXPECT_EQ(S.length(), 0u);
}

//===----------------------------------------------------------------------===//
// dropWhile
//===----------------------------------------------------------------------===//

TEST(StringRefTest, DropWhileDigits) {
  StringRef S("123abc");

  S.dropWhile([](char C) {
    return static_cast<bool>(isdigit(static_cast<unsigned char>(C)));
  });

  EXPECT_TRUE(equals(S, "abc"));
}

TEST(StringRefTest, DropWhileSpaces) {
  StringRef S("   hello");

  S.dropWhile([](char C) { return C == ' '; });

  EXPECT_TRUE(equals(S, "hello"));
}

TEST(StringRefTest, DropWhileNone) {
  StringRef S("hello");

  S.dropWhile([](char C) { return C == ' '; });

  EXPECT_TRUE(equals(S, "hello"));
}

TEST(StringRefTest, DropWhileAll) {
  StringRef S("12345");

  S.dropWhile([](char C) {
    return static_cast<bool>(isdigit(static_cast<unsigned char>(C)));
  });

  EXPECT_EQ(S.length(), 0u);
}

//===----------------------------------------------------------------------===//
// skipSpace
//===----------------------------------------------------------------------===//

TEST(StringRefTest, SkipSpace) {
  StringRef S("   hello");

  S.skipSpace();

  EXPECT_TRUE(equals(S, "hello"));
}

TEST(StringRefTest, SkipSpaceNoSpaces) {
  StringRef S("hello");

  S.skipSpace();

  EXPECT_TRUE(equals(S, "hello"));
}

TEST(StringRefTest, SkipSpaceAllSpaces) {
  StringRef S("     ");

  S.skipSpace();

  EXPECT_EQ(S.length(), 0u);
}

TEST(StringRefTest, SkipSpaceOnlyLeading) {
  StringRef S("  hello world  ");

  S.skipSpace();

  EXPECT_TRUE(equals(S, "hello world  "));
}

TEST(StringRefTest, SkipSpaceWithTabs) {
  StringRef S("\t\n  hello");

  S.skipSpace();

  EXPECT_TRUE(equals(S, "hello"));
}

//===----------------------------------------------------------------------===//
// takeWhile
//===----------------------------------------------------------------------===//

TEST(StringRefTest, TakeWhileDigits) {
  StringRef S("123abc");

  StringRef Digits = S.takeWhile([](char C) {
    return static_cast<bool>(isdigit(static_cast<unsigned char>(C)));
  });

  EXPECT_EQ(Digits.length(), 3u);
  EXPECT_TRUE(equals(Digits, "123"));
  // Original unchanged
  EXPECT_EQ(S.length(), 6u);
}

TEST(StringRefTest, TakeWhileAlpha) {
  StringRef S("hello123");

  StringRef Alpha = S.takeWhile([](char C) {
    return static_cast<bool>(isalpha(static_cast<unsigned char>(C)));
  });

  EXPECT_EQ(Alpha.length(), 5u);
  EXPECT_TRUE(equals(Alpha, "hello"));
}

TEST(StringRefTest, TakeWhileNone) {
  StringRef S("123abc");

  StringRef Result = S.takeWhile([](char C) {
    return static_cast<bool>(isalpha(static_cast<unsigned char>(C)));
  });

  EXPECT_EQ(Result.length(), 0u);
}

TEST(StringRefTest, TakeWhileAll) {
  StringRef S("hello");

  StringRef Result = S.takeWhile([](char C) {
    return static_cast<bool>(isalpha(static_cast<unsigned char>(C)));
  });

  EXPECT_EQ(Result.length(), 5u);
  EXPECT_TRUE(equals(Result, "hello"));
}

//===----------------------------------------------------------------------===//
// Integration / Complex Scenarios
//===----------------------------------------------------------------------===//

TEST(StringRefTest, ParseKeyValuePair) {
  StringRef S("key=value");

  StringRef Key = S.takeWhile([](char C) { return C != '='; });
  S.dropFront(Key.length());
  S.consumeFront("=");

  EXPECT_EQ(Key.length(), 3u);
  EXPECT_TRUE(equals(Key, "key"));
  EXPECT_TRUE(equals(S, "value"));
}

TEST(StringRefTest, ParseCommaSeparated) {
  StringRef S("1,2,3");
  int Values[3] = {0, 0, 0};
  int Count = 0;

  while (S.length() > 0 && Count < 3) {
    S.consumeInteger(Values[Count++]);
    S.consumeFront(",");
  }

  EXPECT_EQ(Count, 3);
  EXPECT_EQ(Values[0], 1);
  EXPECT_EQ(Values[1], 2);
  EXPECT_EQ(Values[2], 3);
}

TEST(StringRefTest, ParseWithWhitespace) {
  StringRef S("  hello  world  ");

  S.skipSpace();
  StringRef Word1 = S.takeWhile([](char C) { return C != ' '; });
  S.dropFront(Word1.length());
  S.skipSpace();
  StringRef Word2 = S.takeWhile([](char C) { return C != ' '; });

  EXPECT_EQ(Word1.length(), 5u);
  EXPECT_TRUE(equals(Word1, "hello"));
  EXPECT_EQ(Word2.length(), 5u);
  EXPECT_TRUE(equals(Word2, "world"));
}

} // namespace
