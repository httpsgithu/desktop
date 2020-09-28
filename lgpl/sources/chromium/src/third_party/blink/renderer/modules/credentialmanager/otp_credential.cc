// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/otp_credential.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
constexpr char kOtpCredentialType2[] = "otp";
}

OTPCredential::OTPCredential(const String& code)
    : Credential(String(), kOtpCredentialType2), code_(code) {}

bool OTPCredential::IsOTPCredential() const {
  return true;
}

}  // namespace blink