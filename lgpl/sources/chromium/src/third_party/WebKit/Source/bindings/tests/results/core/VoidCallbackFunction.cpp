// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "VoidCallbackFunction.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/Assertions.h"

namespace blink {

VoidCallbackFunction::VoidCallbackFunction(ScriptState* scriptState, v8::Local<v8::Function> callback)
    : m_scriptState(scriptState),
    m_callback(scriptState->isolate(), this, callback) {
  DCHECK(!m_callback.isEmpty());
}

DEFINE_TRACE(VoidCallbackFunction) {}

DEFINE_TRACE_WRAPPERS(VoidCallbackFunction) {
  visitor->traceWrappers(m_callback.cast<v8::Value>());
}

bool VoidCallbackFunction::call(ScriptWrappable* scriptWrappable) {
  if (!m_scriptState->contextIsValid())
    return false;

  ExecutionContext* context = m_scriptState->getExecutionContext();
  DCHECK(context);
  if (context->activeDOMObjectsAreSuspended() || context->activeDOMObjectsAreStopped())
    return false;

  if (m_callback.isEmpty())
    return false;

  // TODO(bashi): Make sure that using TrackExceptionState is OK.
  // crbug.com/653769
  TrackExceptionState exceptionState;
  ScriptState::Scope scope(m_scriptState.get());

  v8::Local<v8::Value> thisValue = toV8(scriptWrappable, m_scriptState->context()->Global(), m_scriptState->isolate());

  v8::Local<v8::Value> *argv = nullptr;

  v8::Local<v8::Value> v8ReturnValue;
  v8::TryCatch exceptionCatcher(m_scriptState->isolate());
  exceptionCatcher.SetVerbose(true);

  if (V8ScriptRunner::callFunction(m_callback.newLocal(m_scriptState->isolate()), m_scriptState->getExecutionContext(), thisValue, 0, argv, m_scriptState->isolate()).ToLocal(&v8ReturnValue)) {
    return true;
  }
  return false;
}

}  // namespace blink
