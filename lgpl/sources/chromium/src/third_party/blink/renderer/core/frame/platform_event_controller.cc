// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/platform_event_controller.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

PlatformEventController::PlatformEventController(Document* document)
    : PageVisibilityObserver(document ? document->GetPage() : nullptr),
      has_event_listener_(false),
      is_active_(false),
      document_(document) {}

PlatformEventController::~PlatformEventController() = default;

void PlatformEventController::UpdateCallback() {
  DCHECK(HasLastData());
  DidUpdateData();
}

void PlatformEventController::StartUpdating() {
  if (is_active_ || !document_)
    return;

  if (HasLastData() && !update_callback_handle_.IsActive()) {
    update_callback_handle_ = PostCancellableTask(
        *document_->GetTaskRunner(TaskType::kInternalDefault), FROM_HERE,
        WTF::Bind(&PlatformEventController::UpdateCallback,
                  WrapWeakPersistent(this)));
  }

  RegisterWithDispatcher();
  is_active_ = true;
}

void PlatformEventController::StopUpdating() {
  if (!is_active_)
    return;

  update_callback_handle_.Cancel();
  UnregisterWithDispatcher();
  is_active_ = false;
}

void PlatformEventController::PageVisibilityChanged() {
  if (!has_event_listener_)
    return;

  if (GetPage()->IsPageVisible())
    StartUpdating();
  else
    StopUpdating();
}

void PlatformEventController::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  PageVisibilityObserver::Trace(visitor);
}

}  // namespace blink
