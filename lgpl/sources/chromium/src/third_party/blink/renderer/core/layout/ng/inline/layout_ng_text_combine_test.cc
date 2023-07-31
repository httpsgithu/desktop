// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"

#include <sstream>
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using ::testing::ElementsAre;

class LayoutNGTextCombineTest : public RenderingTest {
 protected:
  std::string AsInkOverflowString(const LayoutBlockFlow& root) {
    std::ostringstream ostream;
    ostream << std::endl;
    for (NGInlineCursor cursor(root); cursor; cursor.MoveToNext()) {
      ostream << cursor.CurrentItem() << std::endl;
      ostream << "                 Rect "
              << cursor.CurrentItem()->RectInContainerFragment() << std::endl;
      ostream << "          InkOverflow " << cursor.CurrentItem()->InkOverflow()
              << std::endl;
      ostream << "      SelfInkOverflow "
              << cursor.CurrentItem()->SelfInkOverflow() << std::endl;
      ostream << "  ContentsInkOverflow "
              << ContentsInkOverflow(*cursor.CurrentItem()) << std::endl;
    }
    return ostream.str();
  }

  static PhysicalRect ContentsInkOverflow(const NGFragmentItem& item) {
    if (const NGPhysicalBoxFragment* box_fragment = item.BoxFragment())
      return box_fragment->ContentsInkOverflow();
    if (!item.HasInkOverflow())
      return PhysicalRect();
    return item.ink_overflow_.Contents(item.InkOverflowType(), item.Size());
  }
};

TEST_F(LayoutNGTextCombineTest, AppendChild) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("combine")->appendChild(Text::Create(GetDocument(), "Z"));
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, BoxBoundary) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>X<b>Y</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "Y"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, DeleteDataToEmpty) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())
      ->deleteData(0, 2, ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1228058
TEST_F(LayoutNGTextCombineTest, ElementRecalcOwnStyle) {
  InsertStyleElement(
      "#root { text-combine-upright: all; writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root><br id=target></div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutNGTextCombine (anonymous)
  |  +--LayoutBR BR id="target"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  // Call |Element::RecalcOwnStyle()| for <br>
  auto& target = *GetElementById("target");
  target.style()->setProperty(GetDocument().GetExecutionContext(), "color",
                              "red", "", ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutNGTextCombine (anonymous)
  |  +--LayoutBR BR id="target" style="color: red;"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1241194
TEST_F(LayoutNGTextCombineTest, HtmlElement) {
  InsertStyleElement(
      "html {"
      "text-combine-upright: all;"
      "writing-mode: vertical-lr;"
      "}");

  // Make |Text| node child in <html> element to call
  // |HTMLHtmlElement::PropagateWritingModeAndDirectionFromBody()|
  GetDocument().documentElement()->appendChild(
      Text::Create(GetDocument(), "X"));

  RunDocumentLifecycle();

  EXPECT_EQ(
      R"DUMP(
LayoutNGBlockFlow HTML
  +--LayoutNGBlockFlow BODY
  +--LayoutNGBlockFlow (anonymous)
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
)DUMP",
      ToSimpleLayoutTree(*GetDocument().documentElement()->GetLayoutObject()));
}

TEST_F(LayoutNGTextCombineTest, InkOverflow) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 100px/110px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=combine>0123456789</c>b</div>");
  const auto& root =
      *To<LayoutBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
{Line #descendants=5 LTR Standard}
                 Rect "0,0 110x300"
          InkOverflow "0,0 110x300"
      SelfInkOverflow "0,0 110x300"
  ContentsInkOverflow "0,0 0x0"
{Text 0-1 LTR Standard}
                 Rect "5,0 100x100"
          InkOverflow "0,0 100x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "0,0 0x0"
{Box #descendants=2 Standard}
                 Rect "5,100 100x100"
          InkOverflow "-5,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 110x100"
{Box #descendants=1 AtomicInlineLTR Standard}
                 Rect "5,100 100x100"
          InkOverflow "-5,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 110x100"
{Text 2-3 LTR Standard}
                 Rect "5,200 100x100"
          InkOverflow "0,0 100x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(root));

  // Note: text item rect has non-scaled size.
  const auto& text_combine = *To<LayoutNGTextCombine>(
      GetElementById("combine")->GetLayoutObject()->SlowFirstChild());
  EXPECT_EQ(R"DUMP(
{Line #descendants=2 LTR Standard}
                 Rect "0,0 100x100"
          InkOverflow "-5,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 110x100"
{Text 0-10 LTR Standard}
                 Rect "0,0 1000x100"
          InkOverflow "0,0 1000x100"
      SelfInkOverflow "0,0 1000x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(text_combine));
}

TEST_F(LayoutNGTextCombineTest, InkOverflowEmphasisMark) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 100px/110px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { -webkit-text-emphasis: dot; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=combine>0123456789</c>b</div>");
  const auto& root =
      *To<LayoutBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
{Line #descendants=5 LTR Standard}
                 Rect "0,0 155x300"
          InkOverflow "0,0 155x300"
      SelfInkOverflow "0,0 155x300"
  ContentsInkOverflow "0,0 0x0"
{Text 0-1 LTR Standard}
                 Rect "5,0 100x100"
          InkOverflow "0,0 150x100"
      SelfInkOverflow "0,0 150x100"
  ContentsInkOverflow "0,0 0x0"
{Box #descendants=2 Standard}
                 Rect "5,100 100x100"
          InkOverflow "-5,0 155x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 155x100"
{Box #descendants=1 AtomicInlineLTR Standard}
                 Rect "5,100 100x100"
          InkOverflow "-5,0 155x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 155x100"
{Text 2-3 LTR Standard}
                 Rect "5,200 100x100"
          InkOverflow "0,0 150x100"
      SelfInkOverflow "0,0 150x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(root));

  // Note: Emphasis mark is part of text-combine box instead of combined text.
  // Note: text item rect has non-scaled size.
  const auto& text_combine = *To<LayoutNGTextCombine>(
      GetElementById("combine")->GetLayoutObject()->SlowFirstChild());
  EXPECT_EQ(R"DUMP(
{Line #descendants=2 LTR Standard}
                 Rect "0,0 100x100"
          InkOverflow "-5,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 110x100"
{Text 0-10 LTR Standard}
                 Rect "0,0 1000x100"
          InkOverflow "0,0 1000x100"
      SelfInkOverflow "0,0 1000x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(text_combine));
}

TEST_F(LayoutNGTextCombineTest, InkOverflowOverline) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 100px/110px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { text-decoration: overline; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=combine>0123456789</c>b</div>");
  const auto& root =
      *To<LayoutBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
{Line #descendants=5 LTR Standard}
                 Rect "0,0 110x300"
          InkOverflow "0,0 115x300"
      SelfInkOverflow "0,0 110x300"
  ContentsInkOverflow "0,0 115x300"
{Text 0-1 LTR Standard}
                 Rect "5,0 100x100"
          InkOverflow "0,0 110x100"
      SelfInkOverflow "0,0 110x100"
  ContentsInkOverflow "0,0 0x0"
{Box #descendants=2 Standard}
                 Rect "5,100 100x100"
          InkOverflow "0,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "0,0 110x100"
{Box #descendants=1 AtomicInlineLTR Standard}
                 Rect "5,100 100x100"
          InkOverflow "0,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "0,0 110x100"
{Text 2-3 LTR Standard}
                 Rect "5,200 100x100"
          InkOverflow "0,0 110x100"
      SelfInkOverflow "0,0 110x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(root));

  const auto& text_combine = *To<LayoutNGTextCombine>(
      GetElementById("combine")->GetLayoutObject()->SlowFirstChild());
  EXPECT_EQ(R"DUMP(
{Line #descendants=2 LTR Standard}
                 Rect "0,0 100x100"
          InkOverflow "0,0 100x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "0,0 0x0"
{Text 0-10 LTR Standard}
                 Rect "0,0 1000x100"
          InkOverflow "0,0 1000x100"
      SelfInkOverflow "0,0 1000x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(text_combine));
}

TEST_F(LayoutNGTextCombineTest, InkOverflowUnderline) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 100px/110px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { text-decoration: underline; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=combine>0123456789</c>b</div>");
  const auto& root =
      *To<LayoutBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
{Line #descendants=5 LTR Standard}
                 Rect "0,0 110x300"
          InkOverflow "-6,0 116x300"
      SelfInkOverflow "0,0 110x300"
  ContentsInkOverflow "-6,0 116x300"
{Text 0-1 LTR Standard}
                 Rect "5,0 100x100"
          InkOverflow "-11,0 111x100"
      SelfInkOverflow "-11,0 111x100"
  ContentsInkOverflow "0,0 0x0"
{Box #descendants=2 Standard}
                 Rect "5,100 100x100"
          InkOverflow "-11,0 111x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-11,0 111x100"
{Box #descendants=1 AtomicInlineLTR Standard}
                 Rect "5,100 100x100"
          InkOverflow "-11,0 111x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-11,0 111x100"
{Text 2-3 LTR Standard}
                 Rect "5,200 100x100"
          InkOverflow "-11,0 111x100"
      SelfInkOverflow "-11,0 111x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(root));

  const auto& text_combine = *To<LayoutNGTextCombine>(
      GetElementById("combine")->GetLayoutObject()->SlowFirstChild());
  EXPECT_EQ(R"DUMP(
{Line #descendants=2 LTR Standard}
                 Rect "0,0 100x100"
          InkOverflow "0,0 100x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "0,0 0x0"
{Text 0-10 LTR Standard}
                 Rect "0,0 1000x100"
          InkOverflow "0,0 1000x100"
      SelfInkOverflow "0,0 1000x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(text_combine));
}

TEST_F(LayoutNGTextCombineTest, InkOverflowWBR) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 100px/110px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=combine>01234<wbr>56789</c>b</div>");
  const auto& root =
      *To<LayoutBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
{Line #descendants=5 LTR Standard}
                 Rect "0,0 110x300"
          InkOverflow "0,0 110x300"
      SelfInkOverflow "0,0 110x300"
  ContentsInkOverflow "0,0 0x0"
{Text 0-1 LTR Standard}
                 Rect "5,0 100x100"
          InkOverflow "0,0 100x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "0,0 0x0"
{Box #descendants=2 Standard}
                 Rect "5,100 100x100"
          InkOverflow "-5,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 110x100"
{Box #descendants=1 AtomicInlineLTR Standard}
                 Rect "5,100 100x100"
          InkOverflow "-5,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 110x100"
{Text 2-3 LTR Standard}
                 Rect "5,200 100x100"
          InkOverflow "0,0 100x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(root));

  // Note: text item rect has non-scaled size.
  const auto& text_combine = *To<LayoutNGTextCombine>(
      GetElementById("combine")->GetLayoutObject()->SlowFirstChild());
  EXPECT_EQ(R"DUMP(
{Line #descendants=4 LTR Standard}
                 Rect "0,0 100x100"
          InkOverflow "-5,0 110x100"
      SelfInkOverflow "0,0 100x100"
  ContentsInkOverflow "-5,0 110x100"
{Text 0-5 LTR Standard}
                 Rect "0,0 500x100"
          InkOverflow "0,0 500x100"
      SelfInkOverflow "0,0 500x100"
  ContentsInkOverflow "0,0 0x0"
{Text 5-6 LTR Standard}
                 Rect "500,0 0x100"
          InkOverflow "0,0 0x100"
      SelfInkOverflow "0,0 0x100"
  ContentsInkOverflow "0,0 0x0"
{Text 6-11 LTR Standard}
                 Rect "500,0 500x100"
          InkOverflow "0,0 500x100"
      SelfInkOverflow "0,0 500x100"
  ContentsInkOverflow "0,0 0x0"
)DUMP",
            AsInkOverflowString(text_combine));
}

TEST_F(LayoutNGTextCombineTest, InsertBefore) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  auto& combine = *GetElementById("combine");
  combine.insertBefore(Text::Create(GetDocument(), "Z"), combine.firstChild());
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "Z"
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1258331
// See also VerticalWritingModeByWBR
TEST_F(LayoutNGTextCombineTest, InsertBR) {
  InsertStyleElement(
      "br { text-combine-upright: all; writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>x</div>");
  auto& root = *GetElementById("root");
  root.insertBefore(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                    root.lastChild());
  RunDocumentLifecycle();

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutBR BR
  +--LayoutText #text "x"
)DUMP",
            ToSimpleLayoutTree(*root.GetLayoutObject()));
}

TEST_F(LayoutNGTextCombineTest, LayoutOverflow) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "  writing-mode: vertical-lr;"
      "  font: 100px/150px Ahem;"
      "}"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML(
      "<div id=t1><tcy>abcefgh</tcy>X</div>"
      "<div id=t2>aX</div>");

  // Layout tree is
  //    LayoutNGBlockFlow {DIV} at (0,0) size 100x200
  //      LayoutInline {TCY} at (0,0) size 100x100
  //        LayoutNGTextCombine (anonymous) at (0,0) size 100x100
  //          LayoutText {#text} at (0,0) size 110x100
  //            text run at (0,0) width 700: "abcefgh"
  //      LayoutText {#text} at (0,100) size 100x100
  //        text run at (0,100) width 100: "X"
  //   LayoutNGBlockFlow {DIV} at (0,200) size 100x200
  //     LayoutText {#text} at (0,0) size 100x200
  //       text run at (0,0) width 200: "aX"

  const auto& sample1 = *To<LayoutBlockFlow>(GetLayoutObjectByElementId("t1"));
  ASSERT_EQ(sample1.PhysicalFragmentCount(), 1u);
  const auto& sample_fragment1 = *sample1.GetPhysicalFragment(0);
  EXPECT_FALSE(sample_fragment1.HasLayoutOverflow());
  EXPECT_EQ(PhysicalSize(150, 200), sample_fragment1.Size());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(), PhysicalSize(150, 200)),
            sample_fragment1.LayoutOverflow());

  const auto& sample2 = *To<LayoutBlockFlow>(GetLayoutObjectByElementId("t2"));
  ASSERT_EQ(sample2.PhysicalFragmentCount(), 1u);
  const auto& sample_fragment2 = *sample2.GetPhysicalFragment(0);
  EXPECT_FALSE(sample_fragment2.HasLayoutOverflow());
  EXPECT_EQ(PhysicalSize(150, 200), sample_fragment2.Size());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(), PhysicalSize(150, 200)),
            sample_fragment2.LayoutOverflow());
}

// http://crbug.com/1223015
TEST_F(LayoutNGTextCombineTest, ListItemStyleToImage) {
  InsertStyleElement(
      "li { text-combine-upright: all; }"
      "ol { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<ol id=root><li></li></ol>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow OL id="root"
  +--LayoutNGListItem LI
  |  +--LayoutNGOutsideListMarker ::marker
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutTextFragment (anonymous) ("1. ")
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  // Change list-marker to use image
  root.style()->setProperty(
      GetDocument().GetExecutionContext(), "list-style-image",
      "url(data:image/"
      "gif;base64,R0lGODlhEAAQAMQAAORHHOVSKudfOulrSOp3WOyDZu6QdvCchPGolfO0o/"
      "XBs/fNwfjZ0frl3/zy7////"
      "wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAkAA"
      "BAALAAAAAAQABAAAAVVICSOZGlCQAosJ6mu7fiyZeKqNKToQGDsM8hBADgUXoGAiqhSvp5QA"
      "nQKGIgUhwFUYLCVDFCrKUE1lBavAViFIDlTImbKC5Gm2hB0SlBCBMQiB0UjIQA7)",
      "", ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow OL id="root" style="list-style-image: url(\"data:image/gif;base64,R0lGODlhEAAQAMQAAORHHOVSKudfOulrSOp3WOyDZu6QdvCchPGolfO0o/XBs/fNwfjZ0frl3/zy7////wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAkAABAALAAAAAAQABAAAAVVICSOZGlCQAosJ6mu7fiyZeKqNKToQGDsM8hBADgUXoGAiqhSvp5QAnQKGIgUhwFUYLCVDFCrKUE1lBavAViFIDlTImbKC5Gm2hB0SlBCBMQiB0UjIQA7\");"
  +--LayoutNGListItem LI
  |  +--LayoutNGOutsideListMarker ::marker
  |  |  +--LayoutImage (anonymous)
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1342520
TEST_F(LayoutNGTextCombineTest, ListMarkerWidthOfSymbol) {
  InsertStyleElement(
      "#root {"
      " text-combine-upright: all;"
      " writing-mode: vertical-lr;"
      " font-size: 1e-7px;"
      "}");
  SetBodyInnerHTML("<li id=root>ab</li>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGListItem LI id="root"
  +--LayoutNGInsideListMarker ::marker
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutTextFragment (anonymous) ("\u2022 ")
  +--LayoutNGTextCombine (anonymous)
  |  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, MultipleTextNode) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>X<!-- -->Y</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  |  |  +--LayoutText #text "Y"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, Nested) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, Outline) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "  writing-mode: vertical-lr;"
      "  text-combine-upright: all;"
      "  font: 100px/150px Ahem;"
      "}"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML(
      "<div id=t1><tcy>abcefgh</tcy>X</div>"
      "<div id=t2>aX</div>");

  // Layout tree is
  //    LayoutNGBlockFlow {DIV} at (0,0) size 100x200
  //      LayoutInline {TCY} at (0,0) size 100x100
  //        LayoutNGTextCombine (anonymous) at (0,0) size 100x100
  //          LayoutText {#text} at (0,0) size 110x100
  //            text run at (0,0) width 700: "abcefgh"
  //      LayoutText {#text} at (0,100) size 100x100
  //        text run at (0,100) width 100: "X"
  //   LayoutNGBlockFlow {DIV} at (0,200) size 100x200
  //     LayoutText {#text} at (0,0) size 100x200
  //       text run at (0,0) width 200: "aX"

  // Sample 1 with text-combine-upright:all
  const auto& sample1 = *GetLayoutObjectByElementId("t1");
  VectorOutlineRectCollector collector;
  sample1.AddOutlineRects(collector, nullptr, PhysicalOffset(),
                          NGOutlineType::kDontIncludeBlockVisualOverflow);
  Vector<PhysicalRect> standard_outlines1 = collector.TakeRects();
  EXPECT_THAT(
      standard_outlines1,
      ElementsAre(PhysicalRect(PhysicalOffset(0, 0), PhysicalSize(150, 200))));

  sample1.AddOutlineRects(collector, nullptr, PhysicalOffset(),
                          NGOutlineType::kIncludeBlockVisualOverflow);
  Vector<PhysicalRect> focus_outlines1 = collector.TakeRects();
  EXPECT_THAT(
      focus_outlines1,
      ElementsAre(
          PhysicalRect(PhysicalOffset(0, 0), PhysicalSize(150, 200)),
          // tcy
          PhysicalRect(PhysicalOffset(25, 0), PhysicalSize(100, 100)),
          PhysicalRect(PhysicalOffset(20, 0), PhysicalSize(110, 100)),
          // "X"
          PhysicalRect(PhysicalOffset(25, 100), PhysicalSize(100, 100)),
          PhysicalRect(PhysicalOffset(25, 100), PhysicalSize(100, 100))));

  // Sample 1 without text-combine-upright:all
  const auto& sample2 = *GetLayoutObjectByElementId("t2");
  sample2.AddOutlineRects(collector, nullptr, PhysicalOffset(),
                          NGOutlineType::kDontIncludeBlockVisualOverflow);
  Vector<PhysicalRect> standard_outlines2 = collector.TakeRects();
  EXPECT_THAT(
      standard_outlines2,
      ElementsAre(PhysicalRect(PhysicalOffset(0, 0), PhysicalSize(150, 100))));

  sample1.AddOutlineRects(collector, nullptr, PhysicalOffset(),
                          NGOutlineType::kIncludeBlockVisualOverflow);
  Vector<PhysicalRect> focus_outlines2 = collector.TakeRects();
  EXPECT_THAT(
      focus_outlines2,
      ElementsAre(
          PhysicalRect(PhysicalOffset(0, 0), PhysicalSize(150, 200)),
          // "a"
          PhysicalRect(PhysicalOffset(25, 0), PhysicalSize(100, 100)),
          PhysicalRect(PhysicalOffset(20, 0), PhysicalSize(110, 100)),
          // "X"
          PhysicalRect(PhysicalOffset(25, 100), PhysicalSize(100, 100)),
          PhysicalRect(PhysicalOffset(25, 100), PhysicalSize(100, 100))));
}

// http://crbug.com/1256783
TEST_F(LayoutNGTextCombineTest, PropageWritingModeFromBodyToHorizontal) {
  InsertStyleElement(
      "body { writing-mode: horizontal-tb; }"
      "html {"
      "text-combine-upright: all;"
      "writing-mode: vertical-lr;"
      "}");

  // Make |Text| node child in <html> element to call
  // |HTMLHtmlElement::PropagateWritingModeAndDirectionFromBody()|
  GetDocument().documentElement()->insertBefore(
      Text::Create(GetDocument(), "X"), GetDocument().body());

  RunDocumentLifecycle();

  EXPECT_EQ(
      R"DUMP(
LayoutNGBlockFlow HTML
  +--LayoutNGBlockFlow (anonymous)
  |  +--LayoutText #text "X"
  +--LayoutNGBlockFlow BODY
)DUMP",
      ToSimpleLayoutTree(*GetDocument().documentElement()->GetLayoutObject()));
}

TEST_F(LayoutNGTextCombineTest, PropageWritingModeFromBodyToVertical) {
  InsertStyleElement(
      "body { writing-mode: vertical-rl; }"
      "html {"
      "text-combine-upright: all;"
      "writing-mode: horizontal-tb;"
      "}");

  // Make |Text| node child in <html> element to call
  // |HTMLHtmlElement::PropagateWritingModeAndDirectionFromBody()|
  GetDocument().documentElement()->insertBefore(
      Text::Create(GetDocument(), "X"), GetDocument().body());

  RunDocumentLifecycle();

  EXPECT_EQ(
      R"DUMP(
LayoutNGBlockFlow HTML
  +--LayoutNGBlockFlow (anonymous)
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  +--LayoutNGBlockFlow BODY
)DUMP",
      ToSimpleLayoutTree(*GetDocument().documentElement()->GetLayoutObject()));
}

// http://crbug.com/1222160
TEST_F(LayoutNGTextCombineTest, RebuildLayoutTreeForDetails) {
  InsertStyleElement(
      "details { text-combine-upright: all; writing-mode: vertical-rl;  }");
  SetBodyInnerHTML("<details id=root open>ab<summary>XY</summary>cd</details>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DETAILS id="root"
  +--LayoutNGListItem SUMMARY
  |  +--LayoutNGInsideListMarker ::marker
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutTextFragment (anonymous) ("\u25BE ")
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutNGBlockFlow (anonymous)
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "ab"
  |  |  +--LayoutText #text "cd"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  // Rebuild layout tree of <details>
  root.style()->setProperty(GetDocument().GetExecutionContext(), "color", "red",
                            "important", ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DETAILS id="root" style="color: red !important;"
  +--LayoutNGListItem SUMMARY
  |  +--LayoutNGInsideListMarker ::marker
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutTextFragment (anonymous) ("\u25BE ")
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutNGBlockFlow (anonymous)
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "ab"
  |  |  +--LayoutText #text "cd"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http;//crbug.com/1233432
TEST_F(LayoutNGTextCombineTest, RemoveBlockChild) {
  InsertStyleElement(
      "div { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<p id=block>XY</p>de</div>");
  auto& root = *GetElementById("root");

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutNGBlockFlow (anonymous)
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "ab"
  +--LayoutNGBlockFlow P id="block"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutNGBlockFlow (anonymous)
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(*root.GetLayoutObject()));

  GetElementById("block")->remove();
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutNGTextCombine (anonymous)
  |  +--LayoutText #text "ab"
  |  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(*root.GetLayoutObject()));
}

TEST_F(LayoutNGTextCombineTest, RemoveChildCombine) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("combine")->remove();
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, RemoveChildToEmpty) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("combine")->firstChild()->remove();
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1227066
TEST_F(LayoutNGTextCombineTest, RemoveChildToOneCombinedText) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root><c>a<b id=t>x</b>z</c></div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutInline C
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "a"
  |  +--LayoutInline B id="t"
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "x"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "z"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("t")->remove();
  RunDocumentLifecycle();

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutInline C
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "a"
  |  |  +--LayoutText #text "z"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1227066
TEST_F(LayoutNGTextCombineTest, ReplaceChildToOneCombinedText) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root><c>a<b id=t>x</b>z</c></div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutInline C
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "a"
  |  +--LayoutInline B id="t"
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "x"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "z"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  auto& target = *GetElementById("t");
  auto& new_text = *Text::Create(GetDocument(), "X");
  target.parentNode()->replaceChild(&new_text, &target);
  RunDocumentLifecycle();

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutInline C
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "a"
  |  |  +--LayoutText #text "X"
  |  |  +--LayoutText #text "z"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, SetDataToEmpty) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())->setData("");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "We should not have a wrapper.";
}

TEST_F(LayoutNGTextCombineTest, SplitText) {
  V8TestingScope scope;

  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())
      ->splitText(1, ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  |  |  +--LayoutText #text "Y"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, SplitTextAtZero) {
  V8TestingScope scope;

  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())
      ->splitText(0, ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no empty LayoutText.";
}

TEST_F(LayoutNGTextCombineTest, SplitTextBeforeBox) {
  V8TestingScope scope;

  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY<b>Z</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())
      ->splitText(1, ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  |  |  +--LayoutText #text "Y"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, StyleToTextCombineUprightAll) {
  InsertStyleElement("div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no wrapper.";

  GetElementById("combine")->setAttribute("style", "text-combine-upright: all");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine" style="text-combine-upright: all"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no wrapper.";
}

TEST_F(LayoutNGTextCombineTest, StyleToTextCombineUprightNone) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("combine")->setAttribute("style",
                                          "text-combine-upright: none");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine" style="text-combine-upright: none"
  |  +--LayoutInline B
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no wrapper.";
}

TEST_F(LayoutNGTextCombineTest, StyleToHorizontalWritingMode) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  root.setAttribute("style", "writing-mode: horizontal-tb");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root" style="writing-mode: horizontal-tb"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no wrapper.";
}

TEST_F(LayoutNGTextCombineTest, StyleToHorizontalWritingModeWithWordBreak) {
  InsertStyleElement(
      "wbr { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root><wbr></div>");
  auto& root = *GetElementById("root");

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutNGTextCombine (anonymous)
  |  +--LayoutWordBreak WBR
)DUMP",
            ToSimpleLayoutTree(*root.GetLayoutObject()));

  root.setAttribute("style", "writing-mode: horizontal-tb");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root" style="writing-mode: horizontal-tb"
  +--LayoutWordBreak WBR
)DUMP",
            ToSimpleLayoutTree(*root.GetLayoutObject()));
}

TEST_F(LayoutNGTextCombineTest, StyleToVerticalWritingMode) {
  InsertStyleElement("c { text-combine-upright: all; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  root.setAttribute("style", "writing-mode: vertical-rl");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root" style="writing-mode: vertical-rl"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1222121
TEST_F(LayoutNGTextCombineTest, VerticalWritingModeByBR) {
  InsertStyleElement(
      "#sample {  text-combine-upright: all; writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<br id=sample>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetDocument().body()->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow BODY
  +--LayoutBR BR id="sample"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1222121
TEST_F(LayoutNGTextCombineTest, VerticalWritingModeByWBR) {
  InsertStyleElement(
      "#sample {  text-combine-upright: all; writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<wbr id=sample>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetDocument().body()->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow BODY
  +--LayoutWordBreak WBR id="sample"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1222069
TEST_F(LayoutNGTextCombineTest, WithBidiControl) {
  InsertStyleElement(
      "c { text-combine-upright: all; -webkit-rtl-ordering: visual; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, WithBR) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY<br>Z</c>de</div>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  |  +--LayoutBR BR
  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1060007
TEST_F(LayoutNGTextCombineTest, WithMarker) {
  InsertStyleElement(
      "li { text-combine-upright: all; }"
      "p {"
      "  counter-increment: my-counter;"
      "  display: list-item;"
      "  writing-mode: vertical-rl;"
      "}"
      "p::marker {"
      "  content: '<' counter(my-counter) '>';"
      "  text-combine-upright: all;"
      "}");
  SetBodyInnerHTML("<p id=root>ab</p>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutNGListItem P id="root"
  +--LayoutNGOutsideListMarker ::marker
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutTextFragment (anonymous) ("<")
  |  |  +--LayoutCounter (anonymous) "1"
  |  |  +--LayoutTextFragment (anonymous) (">")
  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, WithOrderedList) {
  InsertStyleElement(
      "li { text-combine-upright: all; }"
      "ol { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<ol id=root><li>ab</li></ol>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow OL id="root"
  +--LayoutNGListItem LI
  |  +--LayoutNGOutsideListMarker ::marker
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutTextFragment (anonymous) ("1. ")
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, WithQuote) {
  InsertStyleElement(
      "q { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root><q>XY</q></div>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutInline Q
  |  +--LayoutInline ::before
  |  |  +--LayoutQuote (anonymous)
  |  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  |  +--LayoutTextFragment (anonymous) ("\u201C")
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  +--LayoutInline ::after
  |  |  +--LayoutQuote (anonymous)
  |  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  |  +--LayoutTextFragment (anonymous) ("\u201D")
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1223423
TEST_F(LayoutNGTextCombineTest, WithTab) {
  InsertStyleElement(
      "c { text-combine-upright: all; white-space: pre; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>X\tY</c>de</div>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X\tY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1242755
TEST_F(LayoutNGTextCombineTest, WithTextIndent) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 20px/30px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }"
      "#root { text-indent: 100px; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XYZ</c>de</div>");
  const auto& text_xyz = *To<Text>(GetElementById("combine")->firstChild());

  NGInlineCursor cursor;
  cursor.MoveTo(*text_xyz.GetLayoutObject());

  EXPECT_EQ(PhysicalRect(0, 0, 60, 20),
            cursor.Current().RectInContainerFragment());
}

TEST_F(LayoutNGTextCombineTest, WithWordBreak) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY<wbr>Z</c>de</div>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  |  +--LayoutWordBreak WBR
  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// crbug.com/1430617
TEST_F(LayoutNGTextCombineTest, ShouldBeParentOfSvg) {
  SetBodyInnerHTML(R"HTML(
    <div id="root" style="text-combine-upright: all;">
    <svg>
    <text style="writing-mode: vertical-rl;">Text)HTML");

  // Should have no LayoutNGTextCombine.
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root" style="text-combine-upright: all;"
  +--LayoutSVGRoot svg
  |  +--LayoutNGSVGText text style="writing-mode: vertical-rl;"
  |  |  +--LayoutSVGInlineText #text "Text"
)DUMP",
            ToSimpleLayoutTree(*GetLayoutObjectByElementId("root")));
}

TEST_F(LayoutNGTextCombineTest, InHorizontal) {
  InsertStyleElement(
      "div { writing-mode: horizontal-tb; }"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div><tcy id=sample>ab</tcy></div>");
  const auto& sample_layout_object = *GetLayoutObjectByElementId("sample");

  EXPECT_EQ(R"DUMP(
LayoutInline TCY id="sample"
  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
}

TEST_F(LayoutNGTextCombineTest, InVertical) {
  InsertStyleElement(
      "div { writing-mode: vertical-rl; }"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div><tcy id=sample>ab</tcy></div>");
  const auto& sample_layout_object = *GetLayoutObjectByElementId("sample");

  EXPECT_EQ(R"DUMP(
LayoutInline TCY id="sample"
  +--LayoutNGTextCombine (anonymous)
  |  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
}

}  // namespace blink
