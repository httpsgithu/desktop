// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"
#include <cstdint>

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {
namespace {

enum class CookieCacheLookupResult {
  kCacheMissFirstAccess = 0,
  kCacheHitAfterGet = 1,
  kCacheHitAfterSet = 2,
  kCacheMissAfterGet = 3,
  kCacheMissAfterSet = 4,
  kMaxValue = kCacheMissAfterSet,
};

void LogCookieHistogram(const char* prefix,
                        bool cookie_manager_requested,
                        base::TimeDelta elapsed) {
  base::UmaHistogramTimes(
      base::StrCat({prefix, cookie_manager_requested ? "ManagerRequested"
                                                     : "ManagerAvailable"}),
      elapsed);
}

// TODO(crbug.com/1276520): Remove after truncating characters are fully
// deprecated.
bool ContainsTruncatingChar(UChar c) {
  // equivalent to '\x00', '\x0D', or '\x0A'
  return c == '\0' || c == '\r' || c == '\n';
}

}  // namespace

CookieJar::CookieJar(blink::Document* document)
    : backend_(document->GetExecutionContext()), document_(document) {}

CookieJar::~CookieJar() = default;

void CookieJar::Trace(Visitor* visitor) const {
  visitor->Trace(backend_);
  visitor->Trace(document_);
}

void CookieJar::SetCookie(const String& value) {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return;

  base::ElapsedTimer timer;
  bool requested = RequestRestrictedCookieManagerIfNeeded();
  bool site_for_cookies_ok = true;
  bool top_frame_origin_ok = true;
  backend_->SetCookieFromString(
      cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
      document_->GetExecutionContext()->HasStorageAccess(), value,
      &site_for_cookies_ok, &top_frame_origin_ok);
  last_operation_was_set_ = true;
  LogCookieHistogram("Blink.SetCookieTime.", requested, timer.Elapsed());

  // TODO(crbug.com/1276520): Remove after truncating characters are fully
  // deprecated
  if (value.Find(ContainsTruncatingChar) != kNotFound) {
    document_->CountDeprecation(WebFeature::kCookieWithTruncatingChar);
  }

  static bool reported = false;
  if (!site_for_cookies_ok) {
    if (!reported) {
      reported = true;
      SCOPED_CRASH_KEY_STRING256("RCM", "document-site_for_cookies",
                                 document_->SiteForCookies().ToDebugString());
      SCOPED_CRASH_KEY_STRING256(
          "RCM", "document-top_frame_origin",
          document_->TopFrameOrigin()->ToUrlOrigin().GetDebugString());
      // Only origin here, since url is probably way too sensitive.
      SCOPED_CRASH_KEY_STRING256(
          "RCM", "document-origin",
          url::Origin::Create(GURL(cookie_url)).GetDebugString());
      base::debug::DumpWithoutCrashing();
    }
  }
  if (!top_frame_origin_ok) {
    if (!reported) {
      reported = true;
      SCOPED_CRASH_KEY_STRING256("RCM", "document-site_for_cookies",
                                 document_->SiteForCookies().ToDebugString());
      SCOPED_CRASH_KEY_STRING256(
          "RCM", "document-top_frame_origin",
          document_->TopFrameOrigin()->ToUrlOrigin().GetDebugString());
      // Only origin here, since url is probably way too sensitive.
      SCOPED_CRASH_KEY_STRING256(
          "RCM", "document-origin",
          url::Origin::Create(GURL(cookie_url)).GetDebugString());
      base::debug::DumpWithoutCrashing();
    }
  }
}

void CookieJar::OnBackendDisconnect() {
  shared_memory_initialized_ = false;
  last_cookies_hash_.reset();
  last_version_ = network::mojom::blink::kInvalidCookieVersion;
}

String CookieJar::Cookies() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return String();

  base::ElapsedTimer timer;
  bool requested = RequestRestrictedCookieManagerIfNeeded();

  String value;
  base::ReadOnlySharedMemoryRegion new_mapped_region;
  const bool get_version_shared_memory = !shared_memory_initialized_;

  // Store the latest cookie version to update |last_version_| after getting the
  // string.
  const uint64_t new_version = GetSharedCookieVersion();

  backend_->GetCookiesString(
      cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
      document_->GetExecutionContext()->HasStorageAccess(),
      get_version_shared_memory, &new_mapped_region, &value);

  if (!shared_memory_initialized_ && new_mapped_region.IsValid()) {
    mapped_region_ = std::move(new_mapped_region);
    mapping_ = mapped_region_.Map();
    shared_memory_initialized_ = true;
  }

  LogCookieHistogram("Blink.CookiesTime.", requested, timer.Elapsed());
  UpdateCacheAfterGetRequest(cookie_url, value, new_version);

  last_operation_was_set_ = false;
  return value;
}

bool CookieJar::CookiesEnabled() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return false;

  base::ElapsedTimer timer;
  bool requested = RequestRestrictedCookieManagerIfNeeded();
  bool cookies_enabled = false;
  backend_->CookiesEnabledFor(
      cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
      document_->GetExecutionContext()->HasStorageAccess(), &cookies_enabled);
  LogCookieHistogram("Blink.CookiesEnabledTime.", requested, timer.Elapsed());
  return cookies_enabled;
}

void CookieJar::SetCookieManager(
    mojo::PendingRemote<network::mojom::blink::RestrictedCookieManager>
        cookie_manager) {
  backend_.reset();
  backend_.Bind(std::move(cookie_manager),
                document_->GetTaskRunner(TaskType::kInternalDefault));
}

void CookieJar::InvalidateCache() {
  last_cookies_hash_.reset();
}

uint64_t CookieJar::GetSharedCookieVersion() {
  if (shared_memory_initialized_) {
    // Relaxed memory order since only the version is stored within the region
    // and as such is the only data shared between processes. There is no
    // re-ordering to worry about.
    return mapping_.GetMemoryAs<const std::atomic<uint64_t>>()->load(
        std::memory_order_relaxed);
  }
  return network::mojom::blink::kInvalidCookieVersion;
}

bool CookieJar::RequestRestrictedCookieManagerIfNeeded() {
  if (!backend_.is_bound() || !backend_.is_connected()) {
    backend_.reset();

    // Either the backend was never bound or it became unbound. In case we're in
    // the unbound case perform the appropriate cleanup.
    OnBackendDisconnect();

    document_->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        backend_.BindNewPipeAndPassReceiver(
            document_->GetTaskRunner(TaskType::kInternalDefault)));
    return true;
  }
  return false;
}

void CookieJar::UpdateCacheAfterGetRequest(const KURL& cookie_url,
                                           const String& cookie_string,
                                           uint64_t new_version) {
  absl::optional<unsigned> new_hash =
      WTF::HashInts(WTF::GetHash(cookie_url),
                    cookie_string.IsNull() ? 0 : WTF::GetHash(cookie_string));

  CookieCacheLookupResult result =
      CookieCacheLookupResult::kCacheMissFirstAccess;

  // An invalid version means no shared memory communication so assume changes
  // happened.
  const bool cookie_is_unchanged =
      new_version != network::mojom::blink::kInvalidCookieVersion &&
      last_version_ == new_version;

  if (last_cookies_hash_.has_value() && cookie_is_unchanged) {
    if (last_cookies_hash_ == new_hash) {
      result = last_operation_was_set_
                   ? CookieCacheLookupResult::kCacheHitAfterSet
                   : CookieCacheLookupResult::kCacheHitAfterGet;
    } else {
      result = last_operation_was_set_
                   ? CookieCacheLookupResult::kCacheMissAfterSet
                   : CookieCacheLookupResult::kCacheMissAfterGet;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Blink.Experimental.Cookies.CacheLookupResult2",
                            result);

  // Update the version to what it was before getting the string, ignoring any
  // changes that could have happened since then. This ensures as "stale" a
  // version as possible is used. This is the desired effect to avoid inhibiting
  // IPCs when not desired.
  last_version_ = new_version;
  last_cookies_hash_ = new_hash;
}

}  // namespace blink
