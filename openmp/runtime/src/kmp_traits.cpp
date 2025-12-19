/*
 * kmp_traits.cpp -- Handle OpenMP context traits
 *
 * OpenMP 6.0 specifies the following trait sets:
 * - construct
 * - device
 * - target device
 * - implementation
 * - extension
 * - dynamic
 * Currently, the implementation in this file supports traits from the (target)
 * device and implementation trait sets that are relevant for implementing the
 * OMP_DEFAULT_DEVICE and OMP_AVAILABLE_DEVICES environment variables.
 */

//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "kmp_traits.h"
#include "kmp_i18n.h"

using namespace kmp_trait;

// OpenMP trait grammar (in EBNF), currently used for parsing the
// OMP_DEFAULT_DEVICE/OMP_AVAILABLE_DEVICES environment variables
//
// Notes about the grammar:
// - Device traits are going to be translated into device numbers (aka integers)
// later in the runtime. The parser handles device numbers as device traits that
// have already been translated.
// - "*" is also not a trait, strictly speaking. But it's also supported by the
// parser and converted into a "match any" wildcard trait.
// - OpenMP 6.0 explicitly excludes "&&" and "||" from appearing in the same
// grouping level.
// - This grammar currently only supports plain integers for array subsripts /
// sections, no expressions.
// - TODO:
//   - Add support for more traits
//
// TODOs regarding the implementation (not the grammar):
// - Implement array subscript/section parsing
// - Implement grammar TODOs after they have been incorporated into the grammar
//
// list = [clause {',' clause}]
// clause =
//       device_number
//     | "*" [index_expr]
//     | trait_expr_group
//     | trait_expr index_expr
// device_number = ["-"] integer0
// trait_expr_group =
//       trait_expr
//     | trait_expr {"&&" trait_expr}
//     | trait_expr {"||" trait_expr}
// trait_expr =
//       trait_expr_single
//     | trait_expr_group_paren
// trait_expr_single = ["!"] trait
// trait_expr_group_paren = ["!"] "(" trait_expr_group ")"
// trait =
//       "uid" "(" uid_value ")"
// uid_value = (letter | digit0 | symbol) {letter | digit0 | symbol}
//
// index_expr = "[" integer0 "]" | "[" array_section "]"
// array_section =
//       lower_bound ":" length ":" stride
//     | lower_bound ":" length ":"
//     | lower_bound ":" length
//     | lower_bound "::" stride
//     | lower_bound "::"
//     | lower_bound ":"
//     | ":" length ":" stride
//     | ":" length ":"
//     | ":" length
//     | "::" stride
//     | "::"
//     | ":"
// lower_bound = integer0
// length = integer0
// stride = integer
//
// integer0 = 0 | integer
// integer = digit {digit0}
//
// letter =
//       "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L"
//     | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X"
//     | "Y" | "Z" | "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j"
//     | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t" | "u" | "v"
//     | "w" | "x" | "y" | "z"
// digit0 = "0" | digit
// digit = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
// symbol = "-" | "_"

namespace parser {

#define MAX_RECURSION_DEPTH 64

using namespace kmp_trait;

static StringRef consumeUIDValue(StringRef &Scan) {
  Scan.skipSpace();
  StringRef UID = Scan.takeWhile([](char C) {
    return isalnum(static_cast<unsigned char>(C)) || C == '-' || C == '_';
  });
  Scan.dropFront(UID.length());
  if (UID.empty() || !Scan.consumeFront(")"))
    KMP_FATAL(TraitParserInvalidUID, UID.copy());
  return UID;
}

static bool consumeTrait(OMPTraitExprSingle &Expr, StringRef &Scan) {
  Scan.skipSpace();
  if (!Scan.consumeFront("uid("))
    return false;
  StringRef UID = consumeUIDValue(Scan);
  Expr.setTrait(new OMPUIDTrait(UID));
  return true;
}

static bool consumeTraitExprSingle(OMPTraitExprSingle &Expr, StringRef &Scan) {
  StringRef OrigScan = Scan;

  Scan.skipSpace();
  if (Scan.consumeFront("!"))
    Expr.setNegated();
  if (consumeTrait(Expr, Scan))
    return true;
  Scan = OrigScan;
  return false;
}

// forward declaration
static bool consumeTraitExprGroup(OMPTraitExprGroup &Group, StringRef &Scan,
                                  int MaxRecursion);

static bool consumeTraitExprGroupParen(OMPTraitExprGroup &Group,
                                       StringRef &Scan, int MaxRecursion) {
  if (MaxRecursion-- <= 0)
    KMP_FATAL(TraitParserMaxRecursion, MAX_RECURSION_DEPTH);
  StringRef OrigScan = Scan;

  Scan.skipSpace();
  if (Scan.consumeFront("!"))
    Group.setNegated();

  Scan.skipSpace();
  if (!Scan.consumeFront("(") ||
      !consumeTraitExprGroup(Group, Scan, MaxRecursion)) {
    Scan = OrigScan;
    return false;
  }

  Scan.skipSpace();
  if (!Scan.consumeFront(")")) {
    Scan = OrigScan;
    return false;
  }
  return true;
}

static bool consumeTraitExpr(OMPTraitExpr *&Expr, StringRef &Scan,
                             int MaxRecursion) {
  if (MaxRecursion-- <= 0)
    KMP_FATAL(TraitParserMaxRecursion, MAX_RECURSION_DEPTH);

  // Parse a single trait expression
  OMPTraitExprSingle *SingleExpr = new OMPTraitExprSingle();
  if (consumeTraitExprSingle(*SingleExpr, Scan)) {
    Expr = SingleExpr;
    return true;
  }
  delete SingleExpr;

  // Parse a parenthesized group trait expression
  OMPTraitExprGroup *GroupExpr = new OMPTraitExprGroup();
  if (consumeTraitExprGroupParen(*GroupExpr, Scan, MaxRecursion)) {
    Expr = GroupExpr;
    return true;
  }
  delete GroupExpr;

  return false;
}

static bool consumeTraitExprGroup(OMPTraitExprGroup &Group, StringRef &Scan,
                                  int MaxRecursion) {
  if (MaxRecursion-- <= 0)
    KMP_FATAL(TraitParserMaxRecursion, MAX_RECURSION_DEPTH);

  OMPTraitExpr *Expr = nullptr;
  if (!consumeTraitExpr(Expr, Scan, MaxRecursion))
    return false;

  Group.addExpr(Expr);
  const char *Op = nullptr;

  Scan.skipSpace();
  if (Scan.consumeFront("||")) {
    Group.setGroupType(OMPTraitExprGroup::GroupType::OR);
    Op = "||";
  } else if (Scan.consumeFront("&&")) {
    Group.setGroupType(OMPTraitExprGroup::GroupType::AND);
    Op = "&&";
  } else {
    return true; // single trait expression, no group
  }

  // Now that we got an operator, we need at least one more trait expr.
  do {
    if (!consumeTraitExpr(Expr, Scan, MaxRecursion))
      return false;
    Group.addExpr(Expr);
    Scan.skipSpace();
  } while (Scan.consumeFront(Op));

  return true;
}

static bool consumeClause(OMPTraitClause &Clause, StringRef &Scan) {
  StringRef OrigScan = Scan;
  Scan.skipSpace();

  // Parse wildcard "trait"
  if (Scan.consumeFront("*")) {
    Clause.setExpr(new OMPWildcardTrait());
    return true;
  }

  // Parse a literal device number
  int Value;
  if (Scan.consumeInteger(Value)) {
    Clause.setExpr(new OMPLiteralTrait(Value));
    return true;
  }

  // Parse a trait expression group
  OMPTraitExprGroup *Group = new OMPTraitExprGroup();
  if (consumeTraitExprGroup(*Group, Scan, MAX_RECURSION_DEPTH)) {
    Clause.setExpr(Group);
    return true;
  }
  delete Group;

  Scan = OrigScan;
  return false;
}

static bool consumeList(OMPTraitContext &Context, StringRef &Scan) {
  StringRef OrigScan = Scan;
  Scan.skipSpace();

  while (!Scan.empty()) {
    OMPTraitClause *Clause = new OMPTraitClause();
    if (!consumeClause(*Clause, Scan)) {
      delete Clause;
      Scan = OrigScan;
      return false;
    }
    Context.addClause(Clause);
    OrigScan = Scan;

    Scan.skipSpace();
    if (!Scan.consumeFront(",") && !Scan.empty()) {
      Scan = OrigScan;
      return false;
    }
  }

  return true;
}

} // namespace parser

OMPTraitContext *OMPTraitContext::parseFromSpec(StringRef Spec) {
  OMPTraitContext *Context = new OMPTraitContext();
  if (!parser::consumeList(*Context, Spec))
    KMP_FATAL(TraitParserFailed, Spec.copy());
  return Context;
}
