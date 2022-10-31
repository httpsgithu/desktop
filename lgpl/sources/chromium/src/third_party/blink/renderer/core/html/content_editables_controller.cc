/*
 * Copyright (C) 2013 Opera Software AS. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Opera ASA nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/content_editables_controller.h"

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {
const char kContentEditablesSavedContentsSignature[] =
    "Blink's contentEditables saved content";
const int kContentEditablesSavedContentsVersion = 1;
}  // namespace

// static
ContentEditablesState* ContentEditablesState::Create() {
  return MakeGarbageCollected<ContentEditablesState>();
}

void ContentEditablesState::RegisterContentEditableElement(Element* element) {
  content_editables_with_paths_.insert(element, element->GetPath());
}

void ContentEditablesState::UnregisterContentEditableElement(Element* element) {
  content_editables_with_paths_.erase(
      content_editables_with_paths_.find(element));
}

bool ContentEditablesState::IsRegistered(Element* element) {
  return content_editables_with_paths_.Contains(element);
}

void ContentEditablesState::RestoreContentsIn(Element* element) {
  if (!content_editables_with_paths_.Contains(element))
    return;

  HTMLElement* htmlElement = To<HTMLElement>(element);
  DCHECK(htmlElement->contentEditable() == "true" ||
         htmlElement->contentEditable() == "plaintext-only");
  String elementPath = htmlElement->GetPath();

  if (content_editables_with_paths_.at(htmlElement) == elementPath) {
    auto iter = saved_contents_.find(elementPath);
    if (iter != saved_contents_.end()) {
      htmlElement->setInnerHTML(iter.Get()->value,
                                IGNORE_EXCEPTION_FOR_TESTING);
    }
  }
}

Vector<String> ContentEditablesState::ToStateVector() {
  Vector<String> result;
  if (content_editables_with_paths_.size()) {
    result.ReserveInitialCapacity(content_editables_with_paths_.size() * 2 + 2);
    result.push_back(kContentEditablesSavedContentsSignature);
    result.push_back(String::Number(kContentEditablesSavedContentsVersion, 0u));
    for (const auto& iter : content_editables_with_paths_) {
      result.push_back(iter.value);
      result.push_back(To<HTMLElement>(iter.key.Get())->innerHTML());
    }
  }
  return result;
}

void ContentEditablesState::SetContentEditablesContent(
    const Vector<String>& contents) {
  if (contents.size() &&
      contents[0] == kContentEditablesSavedContentsSignature) {
    // i == 1 is version - unused for now.
    for (WTF::wtf_size_t idx = 2; idx < contents.size(); idx += 2) {
      saved_contents_.insert(contents[idx], contents[idx + 1]);
    }
  }
}

ContentEditablesState::ContentEditablesState() {}

ContentEditablesState::~ContentEditablesState() {}

void ContentEditablesState::Trace(Visitor* visitor) const {
  visitor->Trace(content_editables_with_paths_);
}

// static
ContentEditablesController* ContentEditablesController::Create() {
  return MakeGarbageCollected<ContentEditablesController>();
}

ContentEditablesController::ContentEditablesController()
    : state_(ContentEditablesState::Create()) {}

void ContentEditablesController::RegisterContentEditableElement(
    Element* element) {
  if (!RuntimeEnabledFeatures::RestoreContenteditablesStateEnabled())
    return;

  state_->RegisterContentEditableElement(element);
}

void ContentEditablesController::UnregisterContentEditableElement(
    Element* element) {
  state_->UnregisterContentEditableElement(element);
}

bool ContentEditablesController::IsRegistered(Element* element) {
  return state_->IsRegistered(element);
}

void ContentEditablesController::RestoreContentsIn(Element* element) {
  state_->RestoreContentsIn(element);
}

ContentEditablesState* ContentEditablesController::GetContentEditablesState() {
  return state_.Get();
}

void ContentEditablesController::SetContentEditablesContent(
    const Vector<String>& contents) {
  state_->SetContentEditablesContent(contents);
}

void ContentEditablesController::Trace(Visitor* visitor) const {
  visitor->Trace(state_);
}

}  // namespace blink
