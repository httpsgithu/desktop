// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef StringOrArrayBufferOrArrayBufferView_h
#define StringOrArrayBufferOrArrayBufferView_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class TestArrayBuffer;
class TestArrayBufferView;

class CORE_EXPORT StringOrArrayBufferOrArrayBufferView final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  StringOrArrayBufferOrArrayBufferView();
  bool isNull() const { return m_type == SpecificTypeNone; }

  bool isString() const { return m_type == SpecificTypeString; }
  String getAsString() const;
  void setString(String);
  static StringOrArrayBufferOrArrayBufferView fromString(String);

  bool isArrayBuffer() const { return m_type == SpecificTypeArrayBuffer; }
  TestArrayBuffer* getAsArrayBuffer() const;
  void setArrayBuffer(TestArrayBuffer*);
  static StringOrArrayBufferOrArrayBufferView fromArrayBuffer(TestArrayBuffer*);

  bool isArrayBufferView() const { return m_type == SpecificTypeArrayBufferView; }
  TestArrayBufferView* getAsArrayBufferView() const;
  void setArrayBufferView(TestArrayBufferView*);
  static StringOrArrayBufferOrArrayBufferView fromArrayBufferView(TestArrayBufferView*);

  StringOrArrayBufferOrArrayBufferView(const StringOrArrayBufferOrArrayBufferView&);
  ~StringOrArrayBufferOrArrayBufferView();
  StringOrArrayBufferOrArrayBufferView& operator=(const StringOrArrayBufferOrArrayBufferView&);
  DECLARE_TRACE();

 private:
  enum SpecificTypes {
    SpecificTypeNone,
    SpecificTypeString,
    SpecificTypeArrayBuffer,
    SpecificTypeArrayBufferView,
  };
  SpecificTypes m_type;

  String m_string;
  Member<TestArrayBuffer> m_arrayBuffer;
  Member<TestArrayBufferView> m_arrayBufferView;

  friend CORE_EXPORT v8::Local<v8::Value> toV8(const StringOrArrayBufferOrArrayBufferView&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8StringOrArrayBufferOrArrayBufferView final {
 public:
  CORE_EXPORT static void toImpl(v8::Isolate*, v8::Local<v8::Value>, StringOrArrayBufferOrArrayBufferView&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> toV8(const StringOrArrayBufferOrArrayBufferView&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, StringOrArrayBufferOrArrayBufferView& impl) {
  v8SetReturnValue(callbackInfo, toV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<StringOrArrayBufferOrArrayBufferView> {
  CORE_EXPORT static StringOrArrayBufferOrArrayBufferView nativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::StringOrArrayBufferOrArrayBufferView);

#endif  // StringOrArrayBufferOrArrayBufferView_h
