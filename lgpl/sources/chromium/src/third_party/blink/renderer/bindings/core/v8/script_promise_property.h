// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

class ExecutionContext;

// ScriptPromiseProperty is a helper for implementing a DOM attribute (or
// occasionally a method) whose value is a Promise, and the same Promise must be
// returned each time. ScriptPromiseProperty contains multiple promises
// internally, one for each world that accesses the property.
template <typename IDLResolvedType, typename IDLRejectedType>
class ScriptPromiseProperty final
    : public GarbageCollected<
          ScriptPromiseProperty<IDLResolvedType, IDLRejectedType>> {
 public:
  enum State {
    kPending,
    kResolved,
    kRejected,
  };

  // Creates a ScriptPromiseProperty that will create Promises in
  // the specified ExecutionContext for a property of 'holder'
  // (typically ScriptPromiseProperty should be a member of the
  // property holder).
  explicit ScriptPromiseProperty(ExecutionContext* execution_context)
      : execution_context_(execution_context) {}

  ScriptPromiseProperty(const ScriptPromiseProperty&) = delete;
  ScriptPromiseProperty& operator=(const ScriptPromiseProperty&) = delete;

  ScriptPromise<IDLResolvedType> Promise(DOMWrapperWorld& world) {
    if (!GetExecutionContext()) {
      return ScriptPromise<IDLResolvedType>();
    }

    ScriptState* script_state = ToScriptState(execution_context_.Get(), world);

    for (auto& promise : promises_) {
      if (promise.IsAssociatedWith(script_state)) {
        return static_cast<ScriptPromise<IDLResolvedType>&>(promise);
      }
    }

    ScriptState::Scope scope(script_state);

    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLResolvedType>>(
            script_state);
    // ScriptPromiseResolver usually requires a caller to reject it before
    // releasing, but ScriptPromiseProperty doesn't have such a requirement, so
    // suppress the check forcibly.
    resolver->SuppressDetachCheck();
    ScriptPromise<IDLResolvedType> promise = resolver->Promise();
    if (mark_as_handled_)
      promise.MarkAsHandled();
    switch (state_) {
      case kPending:
        resolvers_.push_back(resolver);
        break;
      case kResolved:
        resolver->Resolve(resolved_);
        break;
      case kRejected:
        resolver->template Reject<IDLRejectedType>(rejected_);
        break;
    }
    promises_.push_back(promise);
    return promise;
  }

  template <typename PassResolvedType>
  void Resolve(PassResolvedType value) {
    CHECK(!ScriptForbiddenScope::IsScriptForbidden());
    DCHECK_EQ(GetState(), kPending);
    if (!GetExecutionContext()) {
      return;
    }
    state_ = kResolved;
    resolved_ = value;
    HeapVector<Member<ScriptPromiseResolverBase>> resolvers;
    resolvers.swap(resolvers_);
    for (const Member<ScriptPromiseResolverBase>& resolver : resolvers) {
      resolver->DowncastTo<IDLResolvedType>()->Resolve(value);
    }
  }

  void ResolveWithUndefined() { Resolve(ToV8UndefinedGenerator()); }

  template <typename PassRejectedType>
  void Reject(PassRejectedType value) {
    CHECK(!ScriptForbiddenScope::IsScriptForbidden());
    if (RuntimeEnabledFeatures::BlinkLifecycleScriptForbiddenEnabled()) {
      CHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
    } else {
      DCHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
    }
    DCHECK_EQ(GetState(), kPending);
    if (!GetExecutionContext()) {
      return;
    }
    state_ = kRejected;
    rejected_ = value;
    HeapVector<Member<ScriptPromiseResolverBase>> resolvers;
    resolvers.swap(resolvers_);
    for (const Member<ScriptPromiseResolverBase>& resolver : resolvers) {
      resolver->Reject<IDLRejectedType>(rejected_);
    }
  }

  // Resets this property by unregistering the Promise property from the
  // holder wrapper. Resets the internal state to Pending and clears the
  // resolved and the rejected values.
  void Reset() {
    state_ = kPending;
    resolved_ = DefaultPromiseResultValue<MemberResolvedType>();
    rejected_ = DefaultPromiseResultValue<MemberRejectedType>();
    resolvers_.clear();
    promises_.clear();
  }

  // Mark generated promises as handled to avoid reporting unhandled rejections.
  void MarkAsHandled() {
    mark_as_handled_ = true;
    for (auto& promise : promises_) {
      promise.MarkAsHandled();
    }
  }

  void Trace(Visitor* visitor) const {
    TraceIfNeeded<MemberResolvedType>::Trace(visitor, resolved_);
    TraceIfNeeded<MemberRejectedType>::Trace(visitor, rejected_);
    visitor->Trace(resolvers_);
    visitor->Trace(promises_);
    visitor->Trace(execution_context_);
  }

  State GetState() const { return state_; }

  // DEPRECATED. If client requires execution context, it should figure its own
  // way to get one.
  ExecutionContext* GetExecutionContext() {
    if (!execution_context_ || execution_context_->IsContextDestroyed()) {
      return nullptr;
    }
    return execution_context_.Get();
  }

 private:
  using MemberResolvedType =
      AddMemberIfNeeded<typename IDLTypeToBlinkImplType<IDLResolvedType>::type>;
  using MemberRejectedType =
      AddMemberIfNeeded<typename IDLTypeToBlinkImplType<IDLRejectedType>::type>;

  template <typename T>
  static T DefaultPromiseResultValue() {
    return {};
  }

  template <typename T>
    requires std::derived_from<T, bindings::EnumerationBase>
  static T DefaultPromiseResultValue() {
    return T(static_cast<T::Enum>(0));
  }

  State state_ = kPending;
  MemberResolvedType resolved_{DefaultPromiseResultValue<MemberResolvedType>()};
  MemberRejectedType rejected_{DefaultPromiseResultValue<MemberRejectedType>()};

  // These vectors contain ScriptPromiseResolver<IDLResolvedType> and
  // ScriptPromise<IDLResolvedType>, respectively. We save ~10KB of binary
  // size by storing them as the untemplated base class and downcasting where
  // needed.
  HeapVector<Member<ScriptPromiseResolverBase>> resolvers_;
  HeapVector<ScriptPromiseUntyped> promises_;
  WeakMember<ExecutionContext> const execution_context_;

  bool mark_as_handled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_H_
