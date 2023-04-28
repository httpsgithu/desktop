// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"

#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CSSComputedStyleDeclarationTest : public PageTestBase {};

TEST_F(CSSComputedStyleDeclarationTest, CleanAncestorsNoRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div>
      <div id=dirty></div>
    </div>
    <div>
      <div id=target style='color:green'></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  GetDocument().getElementById("dirty")->setAttribute("style", "color:pink");
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  Element* target = GetDocument().getElementById("target");
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(target);

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(CSSComputedStyleDeclarationTest, CleanShadowAncestorsNoRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div>
      <div id=dirty></div>
    </div>
    <div id=host></div>
  )HTML");

  Element* host = GetDocument().getElementById("host");

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <div id=target style='color:green'></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  GetDocument().getElementById("dirty")->setAttribute("style", "color:pink");
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  Element* target = shadow_root.getElementById("target");
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(target);

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(CSSComputedStyleDeclarationTest, AdjacentInvalidation) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #b { color: red; }
      .test + #b { color: green; }
    </style>
    <div>
      <span id="a"></span>
      <span id="b"></span>
    </div>
    <div id="c"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");
  Element* c = GetDocument().getElementById("c");

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*a));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*b));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*c));

  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(b);

  EXPECT_EQ("rgb(255, 0, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));

  a->classList().Add("test");

  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdateForNode(*a));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdateForNode(*b));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*c));

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));
}

TEST_F(CSSComputedStyleDeclarationTest,
       NoCrashWhenCallingGetPropertyCSSValueWithVariable) {
  UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().body();
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(target);
  ASSERT_TRUE(computed);
  const CSSValue* result =
      computed->GetPropertyCSSValue(CSSPropertyID::kVariable);
  EXPECT_FALSE(result);
  // Don't crash.
}

// https://crbug.com/1115877
TEST_F(CSSComputedStyleDeclarationTest, SVGBlockSizeLayoutDependent) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg viewBox="0 0 400 400">
      <rect width="400" height="400"></rect>
    </svg>
  )HTML");

  Element* rect = GetDocument().QuerySelector("rect");
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(rect);

  EXPECT_EQ("400px", computed->GetPropertyValue(CSSPropertyID::kBlockSize));

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*rect));
  EXPECT_FALSE(rect->NeedsStyleRecalc());
  EXPECT_FALSE(rect->GetLayoutObject()->NeedsLayout());
}

// https://crbug.com/1115877
TEST_F(CSSComputedStyleDeclarationTest, SVGInlineSizeLayoutDependent) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg viewBox="0 0 400 400">
      <rect width="400" height="400"></rect>
    </svg>
  )HTML");

  Element* rect = GetDocument().QuerySelector("rect");
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(rect);

  EXPECT_EQ("400px", computed->GetPropertyValue(CSSPropertyID::kInlineSize));

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*rect));
  EXPECT_FALSE(rect->NeedsStyleRecalc());
  EXPECT_FALSE(rect->GetLayoutObject()->NeedsLayout());
}

TEST_F(CSSComputedStyleDeclarationTest, UseCountComputedAnimationDelayZero) {
  // Disable CSSScrollTimeline, because kAnimationDelay is not supposed to be
  // reachable when this feature is enabled, and we have DCHECKs which enforce
  // this. (We expect kAlternativeAnimationDelay if CSSScrollTimeline
  // enabled).
  ScopedCSSScrollTimelineForTest scroll_timeline_feature(false);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div {
        color: green;
        /* No animation here. */
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  auto* style = MakeGarbageCollected<CSSComputedStyleDeclaration>(div);

  // There is no animation property specified at all, so getting the computed
  // value should not trigger the counter.
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kAnimationDelay));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));

  // Set some animation (without an explicit delay).
  div->SetInlineStyleProperty(CSSPropertyID::kAnimation, "anim linear");
  UpdateAllLifecyclePhasesForTest();
  // It should remain uncounted until we retrieve the computed value.
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kAnimationDelay));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));
  // Accessing kAnimation should also set the counter.
  GetDocument().ClearUseCounterForTesting(
      WebFeature::kCSSGetComputedAnimationDelayZero);
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kAnimation));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));

  // Use-counter should not trigger when there's a non-zero duration.
  GetDocument().ClearUseCounterForTesting(
      WebFeature::kCSSGetComputedAnimationDelayZero);
  div->SetInlineStyleProperty(CSSPropertyID::kAnimation, "anim linear 1s");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kAnimationDelay));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));
}

TEST_F(CSSComputedStyleDeclarationTest,
       UseCountComputedAlternativeAnimationDelayZero) {
  ScopedCSSScrollTimelineForTest scroll_timeline_feature(true);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div {
        color: green;
        /* No animation here. */
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  auto* style = MakeGarbageCollected<CSSComputedStyleDeclaration>(div);

  // There is no animation property specified at all, so getting the computed
  // value should not trigger the counter.
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kAlternativeAnimation));
  EXPECT_TRUE(
      style->GetPropertyCSSValue(CSSPropertyID::kAlternativeAnimationDelay));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));

  // Set some animation (without an explicit delay). We should not count for
  // -alternative-animation[-delay], because those properties are only in
  // use when 'CSSScrollTimeline' is enabled (which is the feature that would
  // ship the change that this use-counter is for in the first place).
  div->SetInlineStyleProperty(CSSPropertyID::kAlternativeAnimation,
                              "anim linear");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kAlternativeAnimation));
  EXPECT_TRUE(
      style->GetPropertyCSSValue(CSSPropertyID::kAlternativeAnimationDelay));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDelayZero));
}

}  // namespace blink
