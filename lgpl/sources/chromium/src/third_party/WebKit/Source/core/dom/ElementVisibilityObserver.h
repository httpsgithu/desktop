// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ElementVisibilityObserver_h
#define ElementVisibilityObserver_h

#include "core/CoreExport.h"
#include "core/dom/IntersectionObserver.h"
#include "platform/heap/Heap.h"
#include "platform/heap/Member.h"

namespace blink {

class Element;

// ElementVisibilityObserver is a helper class to be used to track the
// visibility of an Element in the viewport. Creating an
// ElementVisibilityObserver is a no-op with regards to CPU cycle. The observing
// has be started by calling |start()| and can be stopped with |stop()|.
// When creating an instance, the caller will have to pass a callback taking
// a boolean as an argument. The boolean will be the new visibility state.
// The ElementVisibilityObserver is implemented on top of IntersectionObserver.
// It is a layer meant to simplify the usage for C++ Blink code checking for the
// visibility of an element.
class CORE_EXPORT ElementVisibilityObserver final
    : public GarbageCollectedFinalized<ElementVisibilityObserver> {
  WTF_MAKE_NONCOPYABLE(ElementVisibilityObserver);

 public:
  using VisibilityCallback = Function<void(bool), WTF::SameThreadAffinity>;

  ElementVisibilityObserver(Element*, std::unique_ptr<VisibilityCallback>);
  virtual ~ElementVisibilityObserver();

  void start();
  void stop();

  void deliverObservationsForTesting();

  DECLARE_VIRTUAL_TRACE();

 private:
  class ElementVisibilityCallback;

  void onVisibilityChanged(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  Member<Element> m_element;
  Member<IntersectionObserver> m_intersectionObserver;
  std::unique_ptr<VisibilityCallback> m_callback;
};

}  // namespace blink

#endif  // ElementVisibilityObserver_h
