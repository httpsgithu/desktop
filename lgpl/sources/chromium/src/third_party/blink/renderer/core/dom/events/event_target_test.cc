// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

namespace blink {

class EventTargetTest : public RenderingTest {
 public:
  EventTargetTest() = default;
  ~EventTargetTest() override = default;
};

TEST_F(EventTargetTest, UseCountPassiveTouchEventListener) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(
      "window.addEventListener('touchstart', function() {}, "
      "{passive: true});")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
}

TEST_F(EventTargetTest, UseCountNonPassiveTouchEventListener) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(
      "window.addEventListener('touchstart', function() {}, "
      "{passive: false});")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
}

TEST_F(EventTargetTest, UseCountPassiveTouchEventListenerPassiveNotSpecified) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(
      "window.addEventListener('touchstart', function() {});")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
}

TEST_F(EventTargetTest, UseCountBeforematch) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kBeforematchHandlerRegistered));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
                       const element = document.createElement('div');
                       document.body.appendChild(element);
                       element.addEventListener('beforematch', () => {});
                      )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kBeforematchHandlerRegistered));
}

TEST_F(EventTargetTest, UseCountAbortSignal) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kAddEventListenerWithAbortSignal));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
                       const element = document.createElement('div');
                       const ac = new AbortController();
                       element.addEventListener(
                         'test', () => {}, {signal: ac.signal});
                      )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kAddEventListenerWithAbortSignal));
}

TEST_F(EventTargetTest, UseCountScrollend) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kScrollend));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"HTML(
                       const element = document.createElement('div');
                       element.addEventListener('scrollend', () => {});
                       )HTML")
      ->RunScript(GetDocument().domWindow());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kScrollend));
}

// See https://crbug.com/1357453.
// Tests that we don't crash when adding a unload event handler to a target
// that has no ExecutionContext.
TEST_F(EventTargetTest, UnloadWithoutExecutionContext) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"JS(
      document.createElement("track").track.addEventListener(
          "unload",() => {});
                      )JS")
      ->RunScript(GetDocument().domWindow());
}

}  // namespace blink
