// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "V8TestInterfaceCheckSecurity.h"

#include "bindings/core/v8/BindingSecurity.h"
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
const WrapperTypeInfo V8TestInterfaceCheckSecurity::wrapperTypeInfo = { gin::kEmbedderBlink, V8TestInterfaceCheckSecurity::domTemplate, V8TestInterfaceCheckSecurity::trace, V8TestInterfaceCheckSecurity::traceWrappers, 0, nullptr, "TestInterfaceCheckSecurity", 0, WrapperTypeInfo::WrapperTypeObjectPrototype, WrapperTypeInfo::ObjectClassId, WrapperTypeInfo::NotInheritFromActiveScriptWrappable, WrapperTypeInfo::NotInheritFromEventTarget, WrapperTypeInfo::Independent };
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceCheckSecurity.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// bindings/core/v8/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceCheckSecurity::s_wrapperTypeInfo = V8TestInterfaceCheckSecurity::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappable, TestInterfaceCheckSecurity>::value,
    "TestInterfaceCheckSecurity inherits from ActiveScriptWrappable, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceCheckSecurity::hasPendingActivity),
                 decltype(&ScriptWrappable::hasPendingActivity)>::value,
    "TestInterfaceCheckSecurity is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace TestInterfaceCheckSecurityV8Internal {

static void readonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  // Perform a security check for the receiver object.
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::GetterContext, "TestInterfaceCheckSecurity", "readonlyLongAttribute");
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, exceptionState)) {
    v8SetReturnValueNull(info);
    return;
  }

  v8SetReturnValueInt(info, impl->readonlyLongAttribute());
}

void readonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::readonlyLongAttributeAttributeGetter(info);
}

static void longAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  // Perform a security check for the receiver object.
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::GetterContext, "TestInterfaceCheckSecurity", "longAttribute");
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, exceptionState)) {
    v8SetReturnValueNull(info);
    return;
  }

  v8SetReturnValueInt(info, impl->longAttribute());
}

void longAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::longAttributeAttributeGetter(info);
}

static void longAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  // Perform a security check for the receiver object.
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::SetterContext, "TestInterfaceCheckSecurity", "longAttribute");
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, exceptionState)) {
    v8SetReturnValue(info, v8Value);
    return;
  }

  // Prepare the value to be set.
  int cppValue = toInt32(info.GetIsolate(), v8Value, NormalConversion, exceptionState);
  if (exceptionState.hadException())
    return;

  impl->setLongAttribute(cppValue);
}

void longAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceCheckSecurityV8Internal::longAttributeAttributeSetter(v8Value, info);
}

static void doNotCheckSecurityLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  v8SetReturnValueInt(info, impl->doNotCheckSecurityLongAttribute());
}

void doNotCheckSecurityLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityLongAttributeAttributeGetter(info);
}

static void doNotCheckSecurityLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::SetterContext, "TestInterfaceCheckSecurity", "doNotCheckSecurityLongAttribute");

  // Prepare the value to be set.
  int cppValue = toInt32(info.GetIsolate(), v8Value, NormalConversion, exceptionState);
  if (exceptionState.hadException())
    return;

  impl->setDoNotCheckSecurityLongAttribute(cppValue);
}

void doNotCheckSecurityLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityLongAttributeAttributeSetter(v8Value, info);
}

static void doNotCheckSecurityReadonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  v8SetReturnValueInt(info, impl->doNotCheckSecurityReadonlyLongAttribute());
}

void doNotCheckSecurityReadonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReadonlyLongAttributeAttributeGetter(info);
}

static void doNotCheckSecurityOnSetterLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  v8SetReturnValueInt(info, impl->doNotCheckSecurityOnSetterLongAttribute());
}

void doNotCheckSecurityOnSetterLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityOnSetterLongAttributeAttributeGetter(info);
}

static void doNotCheckSecurityOnSetterLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::SetterContext, "TestInterfaceCheckSecurity", "doNotCheckSecurityOnSetterLongAttribute");

  // Prepare the value to be set.
  int cppValue = toInt32(info.GetIsolate(), v8Value, NormalConversion, exceptionState);
  if (exceptionState.hadException())
    return;

  impl->setDoNotCheckSecurityOnSetterLongAttribute(cppValue);
}

void doNotCheckSecurityOnSetterLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityOnSetterLongAttributeAttributeSetter(v8Value, info);
}

static void doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);

  v8SetReturnValueInt(info, impl->doNotCheckSecurityReplaceableReadonlyLongAttribute());
}

void doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetter(info);
}

static void doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  // Prepare the value to be set.

  v8::Local<v8::String> propertyName = v8AtomicString(info.GetIsolate(), "doNotCheckSecurityReplaceableReadonlyLongAttribute");
  v8CallBoolean(info.Holder()->CreateDataProperty(info.GetIsolate()->GetCurrentContext(), propertyName, v8Value));
}

void doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeSetter(v8Value, info);
}

bool securityCheck(v8::Local<v8::Context> accessingContext, v8::Local<v8::Object> accessedObject, v8::Local<v8::Value> data) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(accessedObject);
  return BindingSecurity::shouldAllowAccessTo(toLocalDOMWindow(toDOMWindow(accessingContext)), impl, BindingSecurity::ErrorReportOption::DoNotReport);
}

static void voidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterfaceCheckSecurity", "voidMethod");
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, exceptionState)) {
    return;
  }

  impl->voidMethod();
}

void voidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::voidMethodMethod(info);
}

static void doNotCheckSecurityVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());

  impl->doNotCheckSecurityVoidMethod();
}

void doNotCheckSecurityVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityVoidMethodMethod(info);
}

static void doNotCheckSecurityVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
  const DOMWrapperWorld& world = DOMWrapperWorld::world(info.GetIsolate()->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interfaceTemplate = data->findInterfaceTemplate(world, &V8TestInterfaceCheckSecurity::wrapperTypeInfo);
  v8::Local<v8::Signature> signature = v8::Signature::New(info.GetIsolate(), interfaceTemplate);

  v8::Local<v8::FunctionTemplate> methodTemplate = data->findOrCreateOperationTemplate(world, &domTemplateKey, TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityVoidMethodMethodCallback, v8Undefined(), signature, 0);
  // Return the function by default, unless the user script has overwritten it.
  v8SetReturnValue(info, methodTemplate->GetFunction(info.GetIsolate()->GetCurrentContext()).ToLocalChecked());

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, BindingSecurity::ErrorReportOption::DoNotReport)) {
    return;
  }

  v8::Local<v8::Value> hiddenValue = V8HiddenValue::getHiddenValue(ScriptState::current(info.GetIsolate()), v8::Local<v8::Object>::Cast(info.Holder()), v8AtomicString(info.GetIsolate(), "doNotCheckSecurityVoidMethod"));
  if (!hiddenValue.IsEmpty()) {
    v8SetReturnValue(info, hiddenValue);
  }
}

void doNotCheckSecurityVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityVoidMethodOriginSafeMethodGetter(info);
}

static void doNotCheckSecurityPerWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());

  impl->doNotCheckSecurityPerWorldBindingsVoidMethod();
}

void doNotCheckSecurityPerWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodMethod(info);
}

static void doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
  const DOMWrapperWorld& world = DOMWrapperWorld::world(info.GetIsolate()->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interfaceTemplate = data->findInterfaceTemplate(world, &V8TestInterfaceCheckSecurity::wrapperTypeInfo);
  v8::Local<v8::Signature> signature = v8::Signature::New(info.GetIsolate(), interfaceTemplate);

  v8::Local<v8::FunctionTemplate> methodTemplate = data->findOrCreateOperationTemplate(world, &domTemplateKey, TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodMethodCallback, v8Undefined(), signature, 0);
  // Return the function by default, unless the user script has overwritten it.
  v8SetReturnValue(info, methodTemplate->GetFunction(info.GetIsolate()->GetCurrentContext()).ToLocalChecked());

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, BindingSecurity::ErrorReportOption::DoNotReport)) {
    return;
  }

  v8::Local<v8::Value> hiddenValue = V8HiddenValue::getHiddenValue(ScriptState::current(info.GetIsolate()), v8::Local<v8::Object>::Cast(info.Holder()), v8AtomicString(info.GetIsolate(), "doNotCheckSecurityPerWorldBindingsVoidMethod"));
  if (!hiddenValue.IsEmpty()) {
    v8SetReturnValue(info, hiddenValue);
  }
}

void doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetter(info);
}

static void doNotCheckSecurityPerWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());

  impl->doNotCheckSecurityPerWorldBindingsVoidMethod();
}

void doNotCheckSecurityPerWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodMethodForMainWorld(info);
}

static void doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterForMainWorld(const v8::PropertyCallbackInfo<v8::Value>& info) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
  const DOMWrapperWorld& world = DOMWrapperWorld::world(info.GetIsolate()->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interfaceTemplate = data->findInterfaceTemplate(world, &V8TestInterfaceCheckSecurity::wrapperTypeInfo);
  v8::Local<v8::Signature> signature = v8::Signature::New(info.GetIsolate(), interfaceTemplate);

  v8::Local<v8::FunctionTemplate> methodTemplate = data->findOrCreateOperationTemplate(world, &domTemplateKey, TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodMethodCallbackForMainWorld, v8Undefined(), signature, 0);
  // Return the function by default, unless the user script has overwritten it.
  v8SetReturnValue(info, methodTemplate->GetFunction(info.GetIsolate()->GetCurrentContext()).ToLocalChecked());

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, BindingSecurity::ErrorReportOption::DoNotReport)) {
    return;
  }

  v8::Local<v8::Value> hiddenValue = V8HiddenValue::getHiddenValue(ScriptState::current(info.GetIsolate()), v8::Local<v8::Object>::Cast(info.Holder()), v8AtomicString(info.GetIsolate(), "doNotCheckSecurityPerWorldBindingsVoidMethod"));
  if (!hiddenValue.IsEmpty()) {
    v8SetReturnValue(info, hiddenValue);
  }
}

void doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallbackForMainWorld(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterForMainWorld(info);
}

static void doNotCheckSecurityUnforgeableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());

  impl->doNotCheckSecurityUnforgeableVoidMethod();
}

void doNotCheckSecurityUnforgeableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityUnforgeableVoidMethodMethod(info);
}

static void doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
  const DOMWrapperWorld& world = DOMWrapperWorld::world(info.GetIsolate()->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interfaceTemplate = data->findInterfaceTemplate(world, &V8TestInterfaceCheckSecurity::wrapperTypeInfo);
  v8::Local<v8::Signature> signature = v8::Signature::New(info.GetIsolate(), interfaceTemplate);

  v8::Local<v8::FunctionTemplate> methodTemplate = data->findOrCreateOperationTemplate(world, &domTemplateKey, TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityUnforgeableVoidMethodMethodCallback, v8Undefined(), signature, 0);
  // Return the function by default, unless the user script has overwritten it.
  v8SetReturnValue(info, methodTemplate->GetFunction(info.GetIsolate()->GetCurrentContext()).ToLocalChecked());

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(info.Holder());
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, BindingSecurity::ErrorReportOption::DoNotReport)) {
    return;
  }

  v8::Local<v8::Value> hiddenValue = V8HiddenValue::getHiddenValue(ScriptState::current(info.GetIsolate()), v8::Local<v8::Object>::Cast(info.Holder()), v8AtomicString(info.GetIsolate(), "doNotCheckSecurityUnforgeableVoidMethod"));
  if (!hiddenValue.IsEmpty()) {
    v8SetReturnValue(info, hiddenValue);
  }
}

void doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetter(info);
}

static void TestInterfaceCheckSecurityOriginSafeMethodSetter(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info) {
  if (!name->IsString())
    return;
  v8::Local<v8::Object> holder = V8TestInterfaceCheckSecurity::findInstanceInPrototypeChain(info.Holder(), info.GetIsolate());
  if (holder.IsEmpty())
    return;
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::toImpl(holder);
  v8::String::Utf8Value attributeName(name);
  ExceptionState exceptionState(ExceptionState::SetterContext, *attributeName, "TestInterfaceCheckSecurity", info.Holder(), info.GetIsolate());
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()), impl, exceptionState)) {
    return;
  }

  V8HiddenValue::setHiddenValue(ScriptState::current(info.GetIsolate()), v8::Local<v8::Object>::Cast(info.Holder()), name.As<v8::String>(), v8Value);
}

void TestInterfaceCheckSecurityOriginSafeMethodSetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info) {
  TestInterfaceCheckSecurityV8Internal::TestInterfaceCheckSecurityOriginSafeMethodSetter(name, v8Value, info);
}

} // namespace TestInterfaceCheckSecurityV8Internal

const V8DOMConfiguration::AccessorConfiguration V8TestInterfaceCheckSecurityAccessors[] = {
    {"readonlyLongAttribute", TestInterfaceCheckSecurityV8Internal::readonlyLongAttributeAttributeGetterCallback, 0, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder},
    {"longAttribute", TestInterfaceCheckSecurityV8Internal::longAttributeAttributeGetterCallback, TestInterfaceCheckSecurityV8Internal::longAttributeAttributeSetterCallback, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder},
    {"doNotCheckSecurityLongAttribute", TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityLongAttributeAttributeGetterCallback, TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityLongAttributeAttributeSetterCallback, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder},
    {"doNotCheckSecurityReadonlyLongAttribute", TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReadonlyLongAttributeAttributeGetterCallback, 0, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder},
    {"doNotCheckSecurityOnSetterLongAttribute", TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityOnSetterLongAttributeAttributeGetterCallback, TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityOnSetterLongAttributeAttributeSetterCallback, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder},
    {"doNotCheckSecurityReplaceableReadonlyLongAttribute", TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetterCallback, TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeSetterCallback, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder},
};

const V8DOMConfiguration::MethodConfiguration V8TestInterfaceCheckSecurityMethods[] = {
    {"voidMethod", TestInterfaceCheckSecurityV8Internal::voidMethodMethodCallback, 0, 0, v8::None, V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder},
};

static void installV8TestInterfaceCheckSecurityTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::initializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceCheckSecurity::wrapperTypeInfo.interfaceName, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceCheckSecurity::internalFieldCount);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);
  // Global object prototype chain consists of Immutable Prototype Exotic Objects
  prototypeTemplate->SetImmutableProto();

  // Global objects are Immutable Prototype Exotic Objects
  instanceTemplate->SetImmutableProto();

  // Register DOM constants, attributes and operations.
  V8DOMConfiguration::installAccessors(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestInterfaceCheckSecurityAccessors, WTF_ARRAY_LENGTH(V8TestInterfaceCheckSecurityAccessors));
  V8DOMConfiguration::installMethods(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestInterfaceCheckSecurityMethods, WTF_ARRAY_LENGTH(V8TestInterfaceCheckSecurityMethods));

  // Cross-origin access check
  instanceTemplate->SetAccessCheckCallback(TestInterfaceCheckSecurityV8Internal::securityCheck, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&V8TestInterfaceCheckSecurity::wrapperTypeInfo)));

  const V8DOMConfiguration::AttributeConfiguration doNotCheckSecurityVoidMethodOriginSafeAttributeConfiguration = {
      "doNotCheckSecurityVoidMethod", TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityVoidMethodOriginSafeMethodGetterCallback, TestInterfaceCheckSecurityV8Internal::TestInterfaceCheckSecurityOriginSafeMethodSetterCallback, 0, 0, nullptr, &V8TestInterfaceCheckSecurity::wrapperTypeInfo, v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder,
  };
  V8DOMConfiguration::installAttribute(isolate, world, instanceTemplate, prototypeTemplate, doNotCheckSecurityVoidMethodOriginSafeAttributeConfiguration);
  const V8DOMConfiguration::AttributeConfiguration doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeAttributeConfiguration = {
      "doNotCheckSecurityPerWorldBindingsVoidMethod", TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallback, TestInterfaceCheckSecurityV8Internal::TestInterfaceCheckSecurityOriginSafeMethodSetterCallback, TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallbackForMainWorld, TestInterfaceCheckSecurityV8Internal::TestInterfaceCheckSecurityOriginSafeMethodSetterCallbackForMainWorld, nullptr, &V8TestInterfaceCheckSecurity::wrapperTypeInfo, v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder,
  };
  V8DOMConfiguration::installAttribute(isolate, world, instanceTemplate, prototypeTemplate, doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeAttributeConfiguration);
  const V8DOMConfiguration::AttributeConfiguration doNotCheckSecurityUnforgeableVoidMethodOriginSafeAttributeConfiguration = {
      "doNotCheckSecurityUnforgeableVoidMethod", TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetterCallback, 0, 0, 0, nullptr, &V8TestInterfaceCheckSecurity::wrapperTypeInfo, v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete), V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnInstance, V8DOMConfiguration::CheckHolder,
  };
  V8DOMConfiguration::installAttribute(isolate, world, instanceTemplate, prototypeTemplate, doNotCheckSecurityUnforgeableVoidMethodOriginSafeAttributeConfiguration);
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceCheckSecurity::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::domClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceCheckSecurityTemplate);
}

bool V8TestInterfaceCheckSecurity::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::from(isolate)->hasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceCheckSecurity::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::from(isolate)->findInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceCheckSecurity* V8TestInterfaceCheckSecurity::toImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? toImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

}  // namespace blink
