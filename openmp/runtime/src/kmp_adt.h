/*
 * kmp_adt.h -- Advanced Data Types used internally
 */

//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __KMP_ADT_H__
#define __KMP_ADT_H__

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <utility>

#include "kmp.h"

/// StringRef is a non-owning string class (similar to llvm::StringRef).
class StringRef final {
  const char *Data;
  size_t Len;

public:
  StringRef(const char *Data) : Data(Data), Len(Data ? strlen(Data) : 0) {
    assert(Data && "StringRef cannot be constructed from nullptr");
  }
  StringRef(const char *Data, size_t Len) : Data(Data), Len(Len) {
    assert(Data && "StringRef cannot be constructed from nullptr");
  }

  StringRef(const StringRef &Other) = default;
  StringRef &operator=(const StringRef &Other) = default;

  // Check if the string starts with the given prefix and remove it from the
  // string afterwards.
  bool consumeFront(StringRef Prefix) {
    if (Len < Prefix.Len)
      return false;
    if (memcmp(Data, Prefix.Data, Prefix.Len) != 0)
      return false;
    Data += Prefix.Len;
    Len -= Prefix.Len;
    return true;
  }

  // Start consuming an integer from the start of the string and remove it from
  // the string afterwards.
  // The maximum integer value that can currently be parsed is INT_MAX - 1.
  bool consumeInteger(int &Value, bool AllowZero = true,
                      bool AllowNegative = false) {
    StringRef Orig = *this; // save state
    bool IsNegative = consumeFront("-");
    if (IsNegative && !AllowNegative) {
      *this = Orig;
      return false;
    }
    size_t NumDigits = countWhile([](char C) {
      return static_cast<bool>(isdigit(static_cast<unsigned char>(C)));
    });
    if (!NumDigits) {
      *this = Orig;
      return false;
    }
    assert(NumDigits <= INT_MAX);
    Value = __kmp_basic_str_to_int(Data, static_cast<int>(NumDigits));
    if (Value == INT_MAX) {
      *this = Orig;
      return false;
    }
    dropFront(NumDigits);
    if (IsNegative)
      Value = -Value;
    if (!AllowZero && Value == 0) {
      *this = Orig;
      return false;
    }
    return true;
  }

  // Get an own duplicate of the string.
  // Must be freed with KMP_INTERNAL_FREE().
  char *copy() const {
    char *Copy = static_cast<char *>(KMP_INTERNAL_MALLOC(Len + 1));
    assert(Copy && "copy() failed to allocate memory");
    memcpy(Copy, Data, Len);
    Copy[Len] = '\0';
    return Copy;
  }

  // Count the number of characters in the string while the predicate returns
  // true.
  size_t countWhile(bool (*Predicate)(char)) const {
    size_t I = 0;
    while (I < Len && Predicate(Data[I]))
      ++I;
    return I;
  }

  // Drop the first n characters from the string.
  void dropFront(size_t N) {
    if (N > Len)
      N = Len;
    Data += N;
    Len -= N;
  }

  // Drop characters from the string while the predicate returns true.
  void dropWhile(bool (*Predicate)(char)) { dropFront(countWhile(Predicate)); }

  // Check if the string is empty.
  bool empty() const { return Len == 0; }

  // Get the length of the string.
  size_t length() const { return Len; }

  // Drop space from the start of the string.
  void skipSpace() {
    dropWhile([](char C) {
      return static_cast<bool>(isspace(static_cast<unsigned char>(C)));
    });
  }

  // Construct a new string with the longest prefix of the original string that
  // satisfies the predicate. Doesn't modify the original string.
  StringRef takeWhile(bool (*Predicate)(char)) const {
    return StringRef(Data, countWhile(Predicate));
  }

  // Iterator support (raw pointers work as iterators for contiguous storage)
  const char *begin() const { return Data; }
  const char *end() const { return Data + Len; }
};

/// Vector is a vector class for managing small vectors.
/// InlineThreshold: Number of elements in the inline array. If exceeded, the
/// vector will grow dynamically.
template <typename T, size_t InlineThreshold = 8> class Vector final {
  static_assert(std::is_copy_constructible_v<T>,
                "T must be copy constructible");
  static_assert(std::is_destructible_v<T>, "T must be destructible");

  T InlineData[InlineThreshold];
  T *Data = InlineData;
  size_t Size = 0;
  size_t Capacity = InlineThreshold;

  void copyData(T *Dst, const T *Src, size_t Size) {
    if constexpr (std::is_trivially_copyable_v<T>) {
      memcpy(Dst, Src, Size * sizeof(T));
    } else {
      for (size_t I = 0; I < Size; I++)
        new (&Dst[I]) T(Src[I]); // copy-construct to memory
    }
  }

  void grow() {
    size_t NewCapacity = Capacity + (Capacity / 2) + 1;
    resize(NewCapacity);
  }

  void init(size_t NewCapacity, const T *Init, size_t NewSize) {
    assert(NewCapacity >= NewSize);
    if (NewCapacity > InlineThreshold)
      resize(NewCapacity);
    if (Init)
      copyData(Data, Init, NewSize);
    Size = NewSize;
  }

  void moveFrom(Vector &&Other) {
    if (Other.Data == Other.InlineData) {
      // Cannot move inline data, must copy.
      init(Other.Capacity, Other.Data, Other.Size);
    } else {
      // Steal dynamic data.
      Data = Other.Data;
      Size = Other.Size;
      Capacity = Other.Capacity;
    }
    Other.reset(false);
  }

  void reset(bool FreeData) {
    if (FreeData && Data != InlineData) {
      clear();
      KMP_INTERNAL_FREE(Data);
    }
    Data = InlineData;
    Size = 0;
    Capacity = InlineThreshold;
  }

  // Resize only changes the capacity, not the size (i.e., the number of
  // actually used elements)
  void resize(size_t NewCapacity) {
    // Currently only supports growing the capacity. (Consequently, doesn't need
    // to worry about going from a dynamic array back to an inline array.)
    assert(NewCapacity > Capacity && "resize() only supports growing");
    Capacity = NewCapacity;
    T *OldData = Data != InlineData ? Data : nullptr;
    Data =
        static_cast<T *>(KMP_INTERNAL_REALLOC(OldData, Capacity * sizeof(T)));
    assert(Data);
    // Copy the data to the new array if we didn't use a dynamic array before.
    if (!OldData)
      copyData(Data, InlineData, Size);
  }

public:
  ~Vector() { reset(true); }

  explicit Vector(size_t Capacity = 0) { init(Capacity, nullptr, 0); }

  Vector(size_t Capacity, const T *Init, size_t Size) {
    init(Capacity, Init, Size);
  }

  Vector(const Vector &Other) { init(Other.Capacity, Other.Data, Other.Size); }

  Vector(Vector &&Other) noexcept { moveFrom(std::move(Other)); }

  Vector &operator=(const Vector &Other) {
    if (this != &Other) {
      reset(true);
      init(Other.Capacity, Other.Data, Other.Size);
    }
    return *this;
  }

  Vector &operator=(Vector &&Other) noexcept {
    if (this != &Other) {
      reset(true);
      moveFrom(std::move(Other));
    }
    return *this;
  }

  // Destroy all elements in the vector. Doesn't free the memory.
  void clear() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (size_t I = 0; I < Size; I++)
        Data[I].~T();
    }
    Size = 0;
  }

  // Check if the vector contains the given value.
  // If a comparator is provided, it will be used to compare the values.
  // Otherwise, the equality operator will be used.
  bool contains(const T &Value,
                bool (*Comp)(const T &, const T &) = nullptr) const {
    for (size_t I = 0; I < Size; I++) {
      if (Comp ? Comp(Data[I], Value) : Data[I] == Value)
        return true;
    }
    return false;
  }

  bool empty() const { return !Size; }

  // Check if the two vectors are equal with set semantics.
  // Current implementation is naive O(n^2) and not optimized for performance.
  bool isSetEqual(const Vector &Other,
                  bool (*Comp)(const T &, const T &) = nullptr) const {
    if (Size != Other.Size)
      return false;
    for (size_t I = 0; I < Size; I++) {
      if (!Other.contains(Data[I], Comp))
        return false;
    }
    return true;
  }

  // Add a new element to the end of the vector.
  void pushBack(const T &Value) {
    if (Size == Capacity)
      grow();
    if constexpr (std::is_trivially_copyable_v<T>) {
      Data[Size++] = Value;
    } else {
      new (&Data[Size++]) T(Value);
    }
  }

  // Reserve space for the given number of elements.
  // (Note: does not shrink the vector.)
  void reserve(size_t NewCapacity) {
    if (NewCapacity > Capacity)
      resize(NewCapacity);
  }

  size_t size() const { return Size; }

  T &operator[](size_t Index) {
    assert(Index < Size && "Index out of bounds");
    return Data[Index];
  }
  const T &operator[](size_t Index) const {
    assert(Index < Size && "Index out of bounds");
    return Data[Index];
  }

  // Iterator support (raw pointers work as iterators for contiguous storage)
  T *begin() { return Data; }
  T *end() { return Data + Size; }
  const T *begin() const { return Data; }
  const T *end() const { return Data + Size; }
};

#endif // __KMP_ADT_H__
