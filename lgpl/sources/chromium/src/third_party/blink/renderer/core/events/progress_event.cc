/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/events/progress_event.h"

#include "third_party/blink/renderer/core/event_names.h"

namespace blink {

ProgressEvent::ProgressEvent()
    : length_computable_(false), loaded_(0), total_(0) {}

ProgressEvent::ProgressEvent(const AtomicString& type,
                             const ProgressEventInit& initializer)
    : Event(type, initializer),
      length_computable_(initializer.lengthComputable()),
      loaded_(initializer.loaded()),
      total_(initializer.total()) {}

ProgressEvent::ProgressEvent(const AtomicString& type,
                             bool length_computable,
                             unsigned long long loaded,
                             unsigned long long total)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      length_computable_(length_computable),
      loaded_(loaded),
      total_(total) {}

const AtomicString& ProgressEvent::InterfaceName() const {
  return EventNames::ProgressEvent;
}

void ProgressEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
}

}  // namespace blink
