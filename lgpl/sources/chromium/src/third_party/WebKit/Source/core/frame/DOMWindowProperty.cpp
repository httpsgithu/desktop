/*
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/DOMWindowProperty.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"

namespace blink {

DOMWindowProperty::DOMWindowProperty(LocalFrame* frame) : m_frame(frame) {
  // FIXME: For now it *is* acceptable for a DOMWindowProperty to be created
  // with a null frame.  See fast/dom/navigator-detached-no-crash.html for the
  // recipe.  We should fix that.  <rdar://problem/11567132>
  if (m_frame) {
    // FIXME: Need to figure out what to do with DOMWindowProperties on
    // remote DOM windows.
    m_frame->localDOMWindow()->registerProperty(this);
  }
}

void DOMWindowProperty::frameDestroyed() {
  // If the property is getting this callback it must have been
  // created with a LocalFrame and it should still have it.
  ASSERT(m_frame);
  m_frame = nullptr;
}

DEFINE_TRACE(DOMWindowProperty) {
  visitor->trace(m_frame);
}

}  // namespace blink
