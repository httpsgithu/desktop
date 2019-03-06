// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/quota/quota_utils.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

void ConnectToQuotaDispatcherHost(
    ExecutionContext* execution_context,
    mojom::blink::QuotaDispatcherHostRequest request) {
  if (auto* interface_provider = execution_context->GetInterfaceProvider())
    interface_provider->GetInterface(std::move(request));
}

}  // namespace blink
