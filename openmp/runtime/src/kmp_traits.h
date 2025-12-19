//===----------- Traits.h - OpenMP context traits -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of OpenMP context traits.
//
//===----------------------------------------------------------------------===//

#ifndef OPENMP_TRAITS_H
#define OPENMP_TRAITS_H

#include "kmp.h"
#include "kmp_adt.h"

namespace kmp_trait {

extern "C" int omp_get_num_devices();
extern "C" const char *omp_get_uid_from_device(int DeviceNum);

class OMPTrait {
protected:
  enum TraitType { WILDCARD_T, LITERAL_T, UID_T };
  TraitType _Type;

  OMPTrait(TraitType Type) : _Type(Type) {}

public:
  virtual ~OMPTrait() = default;

  OMPTrait(const OMPTrait &) = delete;
  OMPTrait(OMPTrait &&) = delete;
  OMPTrait &operator=(const OMPTrait &) = delete;
  OMPTrait &operator=(OMPTrait &&) = delete;

  virtual bool match(int Device) const = 0;

  // Use KMP_INTERNAL_MALLOC/KMP_INTERNAL_FREE for memory management.
  void *operator new(size_t Size) { return KMP_INTERNAL_MALLOC(Size); }
  void operator delete(void *Ptr) { KMP_INTERNAL_FREE(Ptr); }

  virtual bool operator==(const OMPTrait &Other) const {
    return _Type == Other._Type;
  }
};

/// Represents a wildcard trait that matches any device.
class OMPWildcardTrait final : public OMPTrait {
public:
  OMPWildcardTrait() : OMPTrait(WILDCARD_T) {}

  bool match(int Device) const override { return true; }

  bool operator==(const OMPTrait &Other) const override {
    return OMPTrait::operator==(Other);
  }
};

/// Represents a specific device number.
class OMPLiteralTrait final : public OMPTrait {
  int DeviceNum;

public:
  OMPLiteralTrait(int DeviceNum) : OMPTrait(LITERAL_T), DeviceNum(DeviceNum) {}

  bool match(int Device) const override { return DeviceNum == Device; }

  bool operator==(const OMPTrait &Other) const override {
    return OMPTrait::operator==(Other) &&
           DeviceNum == static_cast<const OMPLiteralTrait &>(Other).DeviceNum;
  }
};

/// Represents a specific UID.
/// UID is deliberately not resolved at construction time since libomptarget
/// might not be initialized yet. This is why we delay calls to
/// omp_get_uid_from_device / omp_get_device_from_uid until the trait is
/// evaluated.
class OMPUIDTrait final : public OMPTrait {
  char *UID;
  // Can be used by unit tests to mock omp_get_uid_from_device.
  const char *(*getUIDFromDevice)(int Device) = omp_get_uid_from_device;

public:
  OMPUIDTrait(StringRef UID) : OMPTrait(UID_T), UID(UID.copy()) {}

  ~OMPUIDTrait() override {
    if (UID)
      KMP_INTERNAL_FREE(UID);
  }

  bool match(int Device) const override {
    const char *DeviceUID = getUIDFromDevice(Device);
    if (!DeviceUID || !UID)
      return false;
    return strcmp(DeviceUID, UID) == 0;
  }

  // For testing purposes only: set the function that returns the UID from a
  // device.
  void setUIDFromDevice(const char *(*UIDFromDevice)(int)) {
    getUIDFromDevice = UIDFromDevice;
  }

  bool operator==(const OMPTrait &Other) const override {
    if (!OMPTrait::operator==(Other))
      return false;
    const char *OtherUID = static_cast<const OMPUIDTrait &>(Other).UID;
    return UID && OtherUID ? strcmp(UID, OtherUID) == 0 : UID == OtherUID;
  }
};

/// Represents a collection of traits that are either ANDed or ORed together.
class OMPTraitExpr {
protected:
  enum ExprType { SINGLE_T, GROUP_T };
  ExprType _Type;
  // Determines if the expression is negated (true) or not (false).
  bool Negated = false;
  // Can be used by unit tests to mock omp_get_num_devices.
  int (*getNumDevices)() = omp_get_num_devices;

  OMPTraitExpr(ExprType Type) : _Type(Type) {}
  OMPTraitExpr(ExprType Type, bool Negated) : _Type(Type), Negated(Negated) {}

  virtual bool matchImpl(int Device, int NumDevices) const = 0;

public:
  virtual ~OMPTraitExpr() = default;

  OMPTraitExpr(const OMPTraitExpr &) = delete;
  OMPTraitExpr(OMPTraitExpr &&) = delete;
  OMPTraitExpr &operator=(const OMPTraitExpr &) = delete;
  OMPTraitExpr &operator=(OMPTraitExpr &&) = delete;

  // Returns a sorted set of devices that match the expression.
  Vector<int> evaluate() const {
    Vector<int> Result;
    for (int D = 0, NumDevices = getNumDevices(); D < NumDevices; ++D) {
      if (match(D, NumDevices))
        Result.pushBack(D);
    }
    return Result;
  }

  bool isNegated() const { return Negated; }

  // Check if the device matches the expression.
  bool match(int Device, int NumDevices = -1) const {
    if (NumDevices == -1)
      NumDevices = getNumDevices();
    if (Device < 0 || Device >= NumDevices)
      return false;
    return matchImpl(Device, NumDevices);
  }

  void setNegated(bool Negated = true) { this->Negated = Negated; }

  // For testing purposes only: set the function that returns the number of
  // devices.
  void setNumDevices(int (*NumDevices)()) { getNumDevices = NumDevices; }

  // Use KMP_INTERNAL_MALLOC/KMP_INTERNAL_FREE for memory management.
  void *operator new(size_t Size) { return KMP_INTERNAL_MALLOC(Size); }
  void operator delete(void *Ptr) { KMP_INTERNAL_FREE(Ptr); }

  virtual bool operator==(const OMPTraitExpr &Other) const {
    return _Type == Other._Type && Negated == Other.Negated;
  }
};

class OMPTraitExprSingle final : public OMPTraitExpr {
  OMPTrait *Trait = nullptr;

protected:
  bool matchImpl(int Device, int NumDevices) const override {
    assert(Trait);
    bool Result = Trait->match(Device);
    return Negated ? !Result : Result;
  }

public:
  OMPTraitExprSingle() : OMPTraitExpr(SINGLE_T) {}
  OMPTraitExprSingle(bool Negated) : OMPTraitExpr(SINGLE_T, Negated) {}
  OMPTraitExprSingle(OMPTrait *Trait) : OMPTraitExpr(SINGLE_T), Trait(Trait) {
    assert(Trait && "OMPTraitExprSingle requires a non-null trait");
  }
  ~OMPTraitExprSingle() override { delete Trait; }

  void setTrait(OMPTrait *Trait) {
    assert(Trait);
    if (this->Trait)
      delete this->Trait;
    this->Trait = Trait;
  }

  bool operator==(const OMPTraitExpr &Other) const override {
    if (!OMPTraitExpr::operator==(Other))
      return false;
    const OMPTraitExprSingle &OtherSingle =
        static_cast<const OMPTraitExprSingle &>(Other);
    return Trait && OtherSingle.Trait ? *Trait == *OtherSingle.Trait
                                      : Trait == OtherSingle.Trait;
  }
};

class OMPTraitExprGroup final : public OMPTraitExpr {
public:
  enum GroupType { AND, OR };

private:
  Vector<OMPTraitExpr *> Exprs;
  // Determines if all traits have to match (true) or any of them (false).
  GroupType Type = OR;

protected:
  bool matchImpl(int Device, int NumDevices) const override {
    size_t Matched = 0;
    for (const OMPTraitExpr *Expr : Exprs) {
      if (Expr->match(Device, NumDevices))
        Matched++;
    }
    bool Result = Type == AND ? Matched == Exprs.size() : Matched > 0;
    return Negated ? !Result : Result;
  }

public:
  OMPTraitExprGroup() : OMPTraitExpr(GROUP_T) {}
  OMPTraitExprGroup(bool Negated) : OMPTraitExpr(GROUP_T, Negated) {}
  ~OMPTraitExprGroup() override {
    for (OMPTraitExpr *Expr : Exprs)
      delete Expr;
  }

  void addExpr(OMPTrait *Trait) {
    assert(Trait);
    addExpr(new OMPTraitExprSingle(Trait));
  }
  void addExpr(OMPTraitExpr *Expr) {
    assert(Expr);
    Exprs.pushBack(Expr);
  }

  GroupType getGroupType() const { return Type; }

  void setGroupType(GroupType Type) { this->Type = Type; }

  void setNumDevices(int (*NumDevices)()) {
    OMPTraitExpr::setNumDevices(NumDevices);
    for (OMPTraitExpr *Expr : Exprs)
      Expr->setNumDevices(NumDevices);
  }

  bool operator==(const OMPTraitExpr &Other) const override {
    if (!OMPTraitExpr::operator==(Other))
      return false;
    const OMPTraitExprGroup &OtherGroup =
        static_cast<const OMPTraitExprGroup &>(Other);
    return Exprs.isSetEqual(OtherGroup.Exprs,
                            [](OMPTraitExpr *const &A, OMPTraitExpr *const &B) {
                              return *A == *B;
                            });
  }
};

class OMPTraitClause final {
  OMPTraitExpr *Expr = nullptr;

public:
  OMPTraitClause() = default;
  ~OMPTraitClause() { delete Expr; }

  OMPTraitClause(const OMPTraitClause &) = delete;
  OMPTraitClause(OMPTraitClause &&) = delete;
  OMPTraitClause &operator=(const OMPTraitClause &) = delete;
  OMPTraitClause &operator=(OMPTraitClause &&) = delete;

  OMPTraitExpr *getExpr() { return Expr; }

  bool match(int Device, int NumDevices = -1) const {
    assert(Expr);
    return Expr->match(Device, NumDevices);
  }

  void setExpr(OMPTrait *Trait) {
    assert(Trait);
    if (this->Expr)
      delete this->Expr;
    this->Expr = new OMPTraitExprSingle(Trait);
  }
  void setExpr(OMPTraitExpr *Expr) {
    assert(Expr);
    if (this->Expr)
      delete this->Expr;
    this->Expr = Expr;
  }

  // Use KMP_INTERNAL_MALLOC/KMP_INTERNAL_FREE for memory management.
  void *operator new(size_t Size) { return KMP_INTERNAL_MALLOC(Size); }
  void operator delete(void *Ptr) { KMP_INTERNAL_FREE(Ptr); }

  bool operator==(const OMPTraitClause &Other) const {
    return Expr && Other.Expr ? *Expr == *Other.Expr : Expr == Other.Expr;
  }
};

} // namespace kmp_trait

class OMPTraitContext final {
  using OMPTraitClause = kmp_trait::OMPTraitClause;
  using OMPTraitExpr = kmp_trait::OMPTraitExpr;

  Vector<OMPTraitClause *> Clauses;
  // Can be used by unit tests to mock omp_get_num_devices.
  int (*getNumDevices)() = kmp_trait::omp_get_num_devices;

public:
  OMPTraitContext() = default;
  ~OMPTraitContext() {
    for (OMPTraitClause *Clause : Clauses)
      delete Clause;
  }

  OMPTraitContext(const OMPTraitContext &) = delete;
  OMPTraitContext(OMPTraitContext &&) = delete;
  OMPTraitContext &operator=(const OMPTraitContext &) = delete;
  OMPTraitContext &operator=(OMPTraitContext &&) = delete;

  static OMPTraitContext *parseFromSpec(StringRef Spec);

  void addClause(OMPTraitClause *Clause) {
    assert(Clause);
    Clauses.pushBack(Clause);
  }

  // Returns the list of devices that match the trait specification represented
  // by the context. The list contains devices numbers forming a set and sorted
  // in ascending order.
  Vector<int> evaluate() const {
    Vector<int> Result;
    for (int D = 0; D < getNumDevices(); ++D) {
      if (match(D))
        Result.pushBack(D);
    }
    return Result;
  }

  bool match(int Device) const {
    if (Device < 0 || Device >= getNumDevices())
      return false;
    for (OMPTraitClause *Clause : Clauses) {
      if (Clause->match(Device))
        return true;
    }
    return false;
  }

  // For testing purposes only: set the function that returns the number of
  // devices.
  void setNumDevices(int (*NumDevices)()) {
    getNumDevices = NumDevices;
    for (OMPTraitClause *Clause : Clauses) {
      if (OMPTraitExpr *Expr = Clause->getExpr())
        Expr->setNumDevices(NumDevices);
    }
  }

  // Use KMP_INTERNAL_MALLOC/KMP_INTERNAL_FREE for memory management.
  void *operator new(size_t Size) { return KMP_INTERNAL_MALLOC(Size); }
  void operator delete(void *Ptr) { KMP_INTERNAL_FREE(Ptr); }

  bool operator==(const OMPTraitContext &Other) const {
    auto ClauseComp = [](OMPTraitClause *const &A, OMPTraitClause *const &B) {
      return *A == *B;
    };
    return Clauses.isSetEqual(Other.Clauses, ClauseComp);
  }
};

#endif // OPENMP_TRAITS_H
