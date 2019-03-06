// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/keyboard_lock.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kKeyboardLockFrameDetachedErrorMsg[] =
    "Current frame is detached.";

constexpr char kKeyboardLockPromisePreemptedErrorMsg[] =
    "This request has been superseded by a subsequent lock() method call.";

constexpr char kKeyboardLockNoValidKeyCodesErrorMsg[] =
    "No valid key codes passed into lock().";

constexpr char kKeyboardLockChildFrameErrorMsg[] =
    "lock() must be called from a top-level browsing context.";

constexpr char kKeyboardLockRequestFailedErrorMsg[] =
    "lock() request could not be registered.";

}  // namespace

KeyboardLock::KeyboardLock(ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

KeyboardLock::~KeyboardLock() = default;

ScriptPromise KeyboardLock::lock(ScriptState* state,
                                 const Vector<String>& keycodes) {
  DCHECK(state);

  if (!IsLocalFrameAttached()) {
    return ScriptPromise::RejectWithDOMException(
        state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                    kKeyboardLockFrameDetachedErrorMsg));
  }

  if (!CalledFromSupportedContext(ExecutionContext::From(state))) {
    return ScriptPromise::RejectWithDOMException(
        state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                    kKeyboardLockChildFrameErrorMsg));
  }

  if (!EnsureServiceConnected()) {
    return ScriptPromise::RejectWithDOMException(
        state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                    kKeyboardLockRequestFailedErrorMsg));
  }

  request_keylock_resolver_ = ScriptPromiseResolver::Create(state);
  service_->RequestKeyboardLock(
      keycodes,
      WTF::Bind(&KeyboardLock::LockRequestFinished, WrapPersistent(this),
                WrapPersistent(request_keylock_resolver_.Get())));
  return request_keylock_resolver_->Promise();
}

void KeyboardLock::unlock(ScriptState* state) {
  DCHECK(state);

  if (!CalledFromSupportedContext(ExecutionContext::From(state)))
    return;

  if (!EnsureServiceConnected())
    return;

  service_->CancelKeyboardLock();
}

bool KeyboardLock::IsLocalFrameAttached() {
  if (GetFrame())
    return true;
  return false;
}

bool KeyboardLock::EnsureServiceConnected() {
  if (!service_) {
    LocalFrame* frame = GetFrame();
    if (!frame) {
      return false;
    }
    frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(&service_));
    DCHECK(service_);
  }

  return true;
}

bool KeyboardLock::CalledFromSupportedContext(ExecutionContext* context) {
  DCHECK(context);
  // This API is only accessible from a top level, secure browsing context.
  LocalFrame* frame = GetFrame();
  return frame && frame->IsMainFrame() && context->IsSecureContext();
}

void KeyboardLock::LockRequestFinished(
    ScriptPromiseResolver* resolver,
    mojom::KeyboardLockRequestResult result) {
  DCHECK(request_keylock_resolver_);

  // If |resolver| is not the current promise, then reject the promise.
  if (resolver != request_keylock_resolver_) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kAbortError, kKeyboardLockPromisePreemptedErrorMsg));
    return;
  }

  switch (result) {
    case mojom::KeyboardLockRequestResult::kSuccess:
      request_keylock_resolver_->Resolve();
      break;
    case mojom::KeyboardLockRequestResult::kFrameDetachedError:
      request_keylock_resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kKeyboardLockFrameDetachedErrorMsg));
      break;
    case mojom::KeyboardLockRequestResult::kNoValidKeyCodesError:
      request_keylock_resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidAccessError,
                               kKeyboardLockNoValidKeyCodesErrorMsg));
      break;
    case mojom::KeyboardLockRequestResult::kChildFrameError:
      request_keylock_resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kKeyboardLockChildFrameErrorMsg));
      break;
    case mojom::KeyboardLockRequestResult::kRequestFailedError:
      request_keylock_resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kKeyboardLockRequestFailedErrorMsg));
      break;
  }
  request_keylock_resolver_ = nullptr;
}

void KeyboardLock::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_keylock_resolver_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
