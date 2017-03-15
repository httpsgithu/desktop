// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef LongOrTestDictionary_h
#define LongOrTestDictionary_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8TestDictionary.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT LongOrTestDictionary final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  LongOrTestDictionary();
  bool isNull() const { return m_type == SpecificTypeNone; }

  bool isLong() const { return m_type == SpecificTypeLong; }
  int getAsLong() const;
  void setLong(int);
  static LongOrTestDictionary fromLong(int);

  bool isTestDictionary() const { return m_type == SpecificTypeTestDictionary; }
  const TestDictionary& getAsTestDictionary() const;
  void setTestDictionary(const TestDictionary&);
  static LongOrTestDictionary fromTestDictionary(const TestDictionary&);

  LongOrTestDictionary(const LongOrTestDictionary&);
  ~LongOrTestDictionary();
  LongOrTestDictionary& operator=(const LongOrTestDictionary&);
  DECLARE_TRACE();

 private:
  enum SpecificTypes {
    SpecificTypeNone,
    SpecificTypeLong,
    SpecificTypeTestDictionary,
  };
  SpecificTypes m_type;

  int m_long;
  TestDictionary m_testDictionary;

  friend CORE_EXPORT v8::Local<v8::Value> toV8(const LongOrTestDictionary&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8LongOrTestDictionary final {
 public:
  CORE_EXPORT static void toImpl(v8::Isolate*, v8::Local<v8::Value>, LongOrTestDictionary&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> toV8(const LongOrTestDictionary&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, LongOrTestDictionary& impl) {
  v8SetReturnValue(callbackInfo, toV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<LongOrTestDictionary> {
  CORE_EXPORT static LongOrTestDictionary nativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::LongOrTestDictionary);

#endif  // LongOrTestDictionary_h
