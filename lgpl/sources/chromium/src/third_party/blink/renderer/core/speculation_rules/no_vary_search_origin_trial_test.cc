// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/speculation_rules/stub_speculation_host.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

void CommitTestNavigation(
    LocalFrame& frame,
    const KURL& url,
    const Vector<std::pair<String, String>>& response_headers) {
  auto navigation_params = std::make_unique<WebNavigationParams>();
  navigation_params->url = url;
  WebNavigationParams::FillStaticResponse(navigation_params.get(), "text/html",
                                          "UTF-8", "<!DOCTYPE html>");
  for (const auto& [header, value] : response_headers)
    navigation_params->response.AddHttpHeaderField(header, value);
  frame.Loader().CommitNavigation(std::move(navigation_params), nullptr);
}

HTMLScriptElement* InsertSpeculationRules(Document& document,
                                          const String& speculation_script) {
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, "SpEcUlAtIoNrUlEs");
  script->setText(speculation_script);
  document.head()->appendChild(script);
  return script;
}

// Generated by:
//  tools/origin_trials/generate_token.py --version 3 --expire-days 3650
//  https://speculationrules.test NoVarySearchPrefetch
// Token details:
//  Version: 3
//  Origin: https://speculationrules.test:443
//  Is Subdomain: None
//  Is Third Party: None
//  Usage Restriction: None
//  Feature: NoVarySearchPrefetch
//  Expiry: 1985830923 (2032-12-05 03:42:03 UTC)
//  Signature (Base64):
//  fFyfaSvsR9K2Wqm/Nvo3KWQsdLUaEGHZj+La5IUXRK/LdCrvtdggLOtoQiEZwkL8rJJz3S+/Mfa6I/LOY0KOCA==
[[maybe_unused]] constexpr char kNoVarySearchPrefetchToken[] =
    "A3xcn2kr7EfStlqpvzb6NylkLHS1GhBh2Y/i2uSFF0Svy3Qq77XYICzraEIhGcJC/"
    "KySc90vvzH2uiPyzmNCjggAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9zcGVjdWxhdGlvbnJ1bGV"
    "zLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiTm9WYXJ5U2VhcmNoUHJlZmV0Y2giLCAiZXhwaXJ5I"
    "jogMTk4NTgzMDkyM30=";

TEST(PrefetchNoVarySearchOriginTrialTest, CanEnableFromToken) {
  ScopedTestOriginTrialPolicy using_test_keys;
  DummyPageHolder page_holder;
  LocalFrame& frame = page_holder.GetFrame();

  CommitTestNavigation(frame, KURL("https://speculationrules.test/"),
                       {{"Origin-Trial", kNoVarySearchPrefetchToken}});

  // This should have enabled the origin trial and all its dependent features.
  EXPECT_TRUE(
      RuntimeEnabledFeatures::NoVarySearchPrefetchEnabled(frame.DomWindow()));
  EXPECT_TRUE(RuntimeEnabledFeatures::SpeculationRulesPrefetchProxyEnabled(
      frame.DomWindow()));
  EXPECT_TRUE(RuntimeEnabledFeatures::SpeculationRulesNoVarySearchHintEnabled(
      frame.DomWindow()));
  EXPECT_TRUE(RuntimeEnabledFeatures::SpeculationRulesEagernessEnabled(
      frame.DomWindow()));
}

TEST(PrefetchNoVarySearchOriginTrialTest, DoesNotEnableWithoutToken) {
  ScopedTestOriginTrialPolicy using_test_keys;
  DummyPageHolder page_holder;
  LocalFrame& frame = page_holder.GetFrame();

  // Do not send the Origin-Trial token.
  CommitTestNavigation(frame, KURL("https://speculationrules.test/"), {});

  // This should not have enabled the origin trial.
  EXPECT_FALSE(
      RuntimeEnabledFeatures::NoVarySearchPrefetchEnabled(frame.DomWindow()));
}

void NoVarySearchPrefetchEnabledTest(StubSpeculationHost& speculation_host) {
  DummyPageHolder page_holder;
  LocalFrame& frame = page_holder.GetFrame();
  frame.GetSettings()->SetScriptEnabled(true);

  auto& broker = frame.DomWindow()->GetBrowserInterfaceBroker();
  broker.SetBinderForTesting(
      mojom::blink::SpeculationHost::Name_,
      WTF::BindRepeating(&StubSpeculationHost::BindUnsafe,
                         WTF::Unretained(&speculation_host)));

  base::RunLoop run_loop;
  speculation_host.SetDoneClosure(run_loop.QuitClosure());

  const String speculation_script =
      R"({"prefetch": [
           {"source": "list",
            "urls": ["https://example.com/foo"],
            "requires": ["anonymous-client-ip-when-cross-origin"]}
         ]})";
  {
    auto* script_state = ToScriptStateForMainWorld(&frame);
    v8::MicrotasksScope microtasks_scope(script_state->GetIsolate(),
                                         ToMicrotaskQueue(script_state),
                                         v8::MicrotasksScope::kRunMicrotasks);
    InsertSpeculationRules(page_holder.GetDocument(), speculation_script);
    page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
  }
  run_loop.Run();

  broker.SetBinderForTesting(mojom::blink::SpeculationHost::Name_, {});
}

TEST(PrefetchNoVarySearchOriginTrialTest,
     EnabledNoVarySearchPrefetchInBrowser) {
  ScopedNoVarySearchPrefetchForTest enable_no_vary_search_prefetch_{true};
  StubSpeculationHost speculation_host;
  NoVarySearchPrefetchEnabledTest(speculation_host);
  EXPECT_TRUE(speculation_host.sent_no_vary_search_support_to_browser());
}

TEST(PrefetchNoVarySearchOriginTrialTest,
     DoNotEnableNoVarySearchPrefetchInBrowser) {
  ScopedNoVarySearchPrefetchForTest enable_no_vary_search_prefetch_{false};
  StubSpeculationHost speculation_host;
  NoVarySearchPrefetchEnabledTest(speculation_host);
  EXPECT_FALSE(speculation_host.sent_no_vary_search_support_to_browser());
}

}  // namespace
}  // namespace blink
