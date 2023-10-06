// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/text_auto_space.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

using testing::ElementsAre;
using testing::ElementsAreArray;

class TextAutoSpaceTest : public RenderingTest, ScopedCSSTextAutoSpaceForTest {
public:
  explicit TextAutoSpaceTest() : ScopedCSSTextAutoSpaceForTest(true) {}

  Vector<wtf_size_t> AutoSpaceOffsets(String html,
                                      String container_css = String()) {
    html = String(R"HTML(
      <style>
      #container {
        font-size: 10px;)HTML") +
           container_css + R"HTML(
      }
      </style>
      <div id="container">)HTML" +
           html + "</div>";
    SetBodyInnerHTML(html);
    const auto* container = GetLayoutBlockFlowByElementId("container");
    NGInlineNodeData* node_data = container->GetNGInlineNodeData();
    Vector<wtf_size_t> offsets;
    TextAutoSpace::ApplyIfNeeded(*node_data, &offsets);
    return offsets;
  }
};

TEST_F(TextAutoSpaceTest, Check8Bit) {
  for (UChar32 ch = 0; ch <= std::numeric_limits<uint8_t>::max(); ++ch) {
    EXPECT_NE(TextAutoSpace::GetType(ch), TextAutoSpace::kIdeograph);
  }
}

struct TypeData {
  UChar32 ch;
  TextAutoSpace::CharType type;
} g_type_data[] = {
    {' ', TextAutoSpace::kOther},
    {'0', TextAutoSpace::kLetterOrNumeral},
    {'A', TextAutoSpace::kLetterOrNumeral},
    {u'\u05D0', TextAutoSpace::kLetterOrNumeral},  // Hebrew Letter Alef
    {u'\u0E50', TextAutoSpace::kLetterOrNumeral},  // Thai Digit Zero
    {u'\u3041', TextAutoSpace::kIdeograph},        // Hiragana Letter Small A
    {u'\u30FB', TextAutoSpace::kOther},            // Katakana Middle Dot
    {u'\uFF21', TextAutoSpace::kOther},  // Fullwidth Latin Capital Letter A
    {U'\U00017000', TextAutoSpace::kLetterOrNumeral},  // Tangut Ideograph
    {U'\U00031350', TextAutoSpace::kIdeograph},  // CJK Unified Ideographs H
};

std::ostream& operator<<(std::ostream& ostream, const TypeData& type_data) {
  return ostream << "U+" << std::hex << type_data.ch;
}

class TextAutoSpaceTypeTest : public testing::Test,
                              public testing::WithParamInterface<TypeData> {};

INSTANTIATE_TEST_SUITE_P(TextAutoSpaceTest,
                         TextAutoSpaceTypeTest,
                         testing::ValuesIn(g_type_data));

TEST_P(TextAutoSpaceTypeTest, Char) {
  const auto& data = GetParam();
  EXPECT_EQ(TextAutoSpace::GetType(data.ch), data.type);
}

// Test the optimizations in `ApplyIfNeeded` don't affect results.
TEST_F(TextAutoSpaceTest, NonHanIdeograph) {
  // For boundary-check, extend the range by 1 to lower and to upper.
  for (UChar ch = TextAutoSpace::kNonHanIdeographMin - 1;
       ch <= TextAutoSpace::kNonHanIdeographMax + 1; ++ch) {
    StringBuilder builder;
    builder.Append("X");
    builder.Append(ch);
    builder.Append("X");
    const String html = builder.ToString();
    Vector<wtf_size_t> offsets = AutoSpaceOffsets(html);
    TextAutoSpace::CharType type = TextAutoSpace::GetType(ch);
    if (type == TextAutoSpace::kIdeograph) {
      EXPECT_THAT(offsets, ElementsAre(1, 2)) << String::Format("U+%04X", ch);
    } else {
      EXPECT_THAT(offsets, ElementsAre()) << String::Format("U+%04X", ch);
    }
  }
}

struct HtmlData {
  const UChar* html;
  std::vector<wtf_size_t> offsets;
  const char* container_css = nullptr;
} g_html_data[] = {
    {u"ああああ", {}},
    {u"English only", {}},
    {u"Abcあああ", {3}},
    {u"123あああ", {3}},
    {u"あああAbc", {3}},
    {u"あああ123", {3}},
    {u"ああAああ", {2, 3}},
    {u"ああ1ああ", {2, 3}},
    {u"ああAbcああ", {2, 5}},
    {u"ああA12ああ", {2, 5}},
    {u"ああ123ああ", {2, 5}},
    {u"あ\U000739AD", {}},
    {u"<span>ああ</span>Aああ", {2, 3}},
    {u"<span>ああA</span>ああ", {2, 3}},
    {u"ああ<span>A</span>ああ", {2, 3}},
    {u"ああ<span>Aああ</span>", {2, 3}},
    {u"ああ 12 ああ", {}},
    {u"あ<span style='text-autospace: no-autospace'>1</span>2", {}},
    {u"あ<span style='text-autospace: no-autospace'>あ</span>2", {2}},
    {u"あAあ", {}, "writing-mode: vertical-rl; text-orientation: upright"},
    {u"あ1あ", {}, "writing-mode: vertical-rl; text-orientation: upright"},
    {u"あ<span style='text-orientation: upright'>1</span>あ",
     {},
     "writing-mode: vertical-rl"},
};
class HtmlTest : public TextAutoSpaceTest,
                 public testing::WithParamInterface<HtmlData> {};
INSTANTIATE_TEST_SUITE_P(TextAutoSpaceTest,
                         HtmlTest,
                         testing::ValuesIn(g_html_data));

TEST_P(HtmlTest, Apply) {
  const auto& test = GetParam();
  Vector<wtf_size_t> offsets = AutoSpaceOffsets(test.html, test.container_css);
  EXPECT_THAT(offsets, ElementsAreArray(test.offsets));
}

}  // namespace

}  // namespace blink
