// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_synthesis_error_event.h"

namespace blink {

// static
SpeechSynthesisErrorEvent* SpeechSynthesisErrorEvent::Create(
    const AtomicString& type,
    const SpeechSynthesisErrorEventInit& init) {
  return new SpeechSynthesisErrorEvent(type, init);
}

SpeechSynthesisErrorEvent::SpeechSynthesisErrorEvent(
    const AtomicString& type,
    const SpeechSynthesisErrorEventInit& init)
    : SpeechSynthesisEvent(type,
                           init.utterance(),
                           init.charIndex(),
                           init.elapsedTime(),
                           init.name()),
      error_(init.error()) {}

}  // namespace blink
