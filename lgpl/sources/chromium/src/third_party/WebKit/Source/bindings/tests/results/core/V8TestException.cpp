// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "V8TestException.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "wtf/GetPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestException::wrapperTypeInfo = { gin::kEmbedderBlink, V8TestException::domTemplate, V8TestException::trace, V8TestException::traceWrappers, 0, nullptr, "TestException", 0, WrapperTypeInfo::WrapperTypeExceptionPrototype, WrapperTypeInfo::ObjectClassId, WrapperTypeInfo::NotInheritFromActiveScriptWrappable, WrapperTypeInfo::NotInheritFromEventTarget, WrapperTypeInfo::Independent };
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestException.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// bindings/core/v8/ScriptWrappable.h.
const WrapperTypeInfo& TestException::s_wrapperTypeInfo = V8TestException::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappable, TestException>::value,
    "TestException inherits from ActiveScriptWrappable, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestException::hasPendingActivity),
                 decltype(&ScriptWrappable::hasPendingActivity)>::value,
    "TestException is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace TestExceptionV8Internal {

static void readonlyUnsignedShortAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestException* impl = V8TestException::toImpl(holder);

  v8SetReturnValueUnsigned(info, impl->readonlyUnsignedShortAttribute());
}

void readonlyUnsignedShortAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestExceptionV8Internal::readonlyUnsignedShortAttributeAttributeGetter(info);
}

static void readonlyStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestException* impl = V8TestException::toImpl(holder);

  v8SetReturnValueString(info, impl->readonlyStringAttribute(), info.GetIsolate());
}

void readonlyStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestExceptionV8Internal::readonlyStringAttributeAttributeGetter(info);
}

static void toStringMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestException* impl = V8TestException::toImpl(info.Holder());

  v8SetReturnValueString(info, impl->toString(), info.GetIsolate());
}

void toStringMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestExceptionV8Internal::toStringMethod(info);
}

static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ConstructionContext, "TestException");

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(1, info.Length()));
    return;
  }

  unsigned argument;
  argument = toUInt16(info.GetIsolate(), info[0], NormalConversion, exceptionState);
  if (exceptionState.hadException())
    return;

  TestException* impl = TestException::create(argument);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->associateWithWrapper(info.GetIsolate(), &V8TestException::wrapperTypeInfo, wrapper);
  v8SetReturnValue(info, wrapper);
}

} // namespace TestExceptionV8Internal

const V8DOMConfiguration::AccessorConfiguration V8TestExceptionAccessors[] = {
    {"readonlyUnsignedShortAttribute", TestExceptionV8Internal::readonlyUnsignedShortAttributeAttributeGetterCallback, 0, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnPrototype, V8DOMConfiguration::CheckHolder},
    {"readonlyStringAttribute", TestExceptionV8Internal::readonlyStringAttributeAttributeGetterCallback, 0, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnPrototype, V8DOMConfiguration::CheckHolder},
};

const V8DOMConfiguration::MethodConfiguration V8TestExceptionMethods[] = {
    {"toString", TestExceptionV8Internal::toStringMethodCallback, 0, 0, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnPrototype, V8DOMConfiguration::CheckHolder},
};

void V8TestException::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (!info.IsConstructCall()) {
    V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::constructorNotCallableAsFunction("TestException"));
    return;
  }

  if (ConstructorMode::current(info.GetIsolate()) == ConstructorMode::WrapExistingObject) {
    v8SetReturnValue(info, info.Holder());
    return;
  }

  TestExceptionV8Internal::constructor(info);
}

static void installV8TestExceptionTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::initializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestException::wrapperTypeInfo.interfaceName, v8::Local<v8::FunctionTemplate>(), V8TestException::internalFieldCount);
  interfaceTemplate->SetCallHandler(V8TestException::constructorCallback);
  interfaceTemplate->SetLength(1);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);
  // Register DOM constants, attributes and operations.
  const V8DOMConfiguration::ConstantConfiguration V8TestExceptionConstants[] = {
      {"UNSIGNED_SHORT_CONSTANT", 1, 0, V8DOMConfiguration::ConstantTypeUnsignedShort},
  };
  V8DOMConfiguration::installConstants(isolate, interfaceTemplate, prototypeTemplate, V8TestExceptionConstants, WTF_ARRAY_LENGTH(V8TestExceptionConstants));
  V8DOMConfiguration::installAccessors(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestExceptionAccessors, WTF_ARRAY_LENGTH(V8TestExceptionAccessors));
  V8DOMConfiguration::installMethods(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestExceptionMethods, WTF_ARRAY_LENGTH(V8TestExceptionMethods));
}

v8::Local<v8::FunctionTemplate> V8TestException::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::domClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestExceptionTemplate);
}

bool V8TestException::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::from(isolate)->hasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestException::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::from(isolate)->findInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestException* V8TestException::toImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? toImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

}  // namespace blink
