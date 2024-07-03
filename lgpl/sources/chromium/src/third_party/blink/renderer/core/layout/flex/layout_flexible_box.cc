// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/flex/flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

LayoutFlexibleBox::LayoutFlexibleBox(Element* element) : LayoutBlock(element) {}

bool LayoutFlexibleBox::HasTopOverflow() const {
  const auto& style = StyleRef();
  bool is_wrap_reverse = StyleRef().FlexWrap() == EFlexWrap::kWrapReverse;
  if (style.IsHorizontalWritingMode()) {
    return style.ResolvedIsColumnReverseFlexDirection() ||
           (style.ResolvedIsRowFlexDirection() && is_wrap_reverse);
  }
  return style.IsLeftToRightDirection() ==
         (style.ResolvedIsRowReverseFlexDirection() ||
          (style.ResolvedIsColumnFlexDirection() && is_wrap_reverse));
}

bool LayoutFlexibleBox::HasLeftOverflow() const {
  const auto& style = StyleRef();
  bool is_wrap_reverse = StyleRef().FlexWrap() == EFlexWrap::kWrapReverse;
  if (style.IsHorizontalWritingMode()) {
    return style.IsLeftToRightDirection() ==
           (style.ResolvedIsRowReverseFlexDirection() ||
            (style.ResolvedIsColumnFlexDirection() && is_wrap_reverse));
  }
  return (style.GetWritingMode() == WritingMode::kVerticalLr) ==
         (style.ResolvedIsColumnReverseFlexDirection() ||
          (style.ResolvedIsRowFlexDirection() && is_wrap_reverse));
}

namespace {

void MergeAnonymousFlexItems(LayoutObject* remove_child) {
  // When we remove a flex item, and the previous and next siblings of the item
  // are text nodes wrapped in anonymous flex items, the adjacent text nodes
  // need to be merged into the same flex item.
  LayoutObject* prev = remove_child->PreviousSibling();
  if (!prev || !prev->IsAnonymousBlock())
    return;
  LayoutObject* next = remove_child->NextSibling();
  if (!next || !next->IsAnonymousBlock())
    return;
  To<LayoutBoxModelObject>(next)->MoveAllChildrenTo(
      To<LayoutBoxModelObject>(prev));
  next->Destroy();
}

}  // namespace

bool LayoutFlexibleBox::IsChildAllowed(LayoutObject* object,
                                       const ComputedStyle& style) const {
  const auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  if (UNLIKELY(select && select->UsesMenuList())) {
    if (select->IsAppearanceBaseSelect()) {
      CHECK(RuntimeEnabledFeatures::StylableSelectEnabled());
      if (IsA<HTMLOptionElement>(object->GetNode())) {
        // TODO(crbug.com/1511354): Remove this when <option>s are slotted into
        // the UA <datalist>, which will be hidden by default as a popover.
        return false;
      }
      // For appearance:base-select <select>, we want to render all children.
      // However, the InnerElement is only used for rendering in
      // appearance:auto, so don't include that one.
      return object->GetNode() != &select->InnerElementForAppearanceAuto();
    } else {
      // For a size=1 appearance:auto <select>, we only render the active option
      // label through the InnerElement. We do not allow adding layout objects
      // for options and optgroups.
      return object->GetNode() == &select->InnerElementForAppearanceAuto();
    }
  }
  return LayoutBlock::IsChildAllowed(object, style);
}

void LayoutFlexibleBox::SetNeedsLayoutForDevtools() {
  SetNeedsLayout(layout_invalidation_reason::kDevtools);
  SetNeedsDevtoolsInfo(true);
}

const DevtoolsFlexInfo* LayoutFlexibleBox::FlexLayoutData() const {
  const wtf_size_t fragment_count = PhysicalFragmentCount();
  DCHECK_GE(fragment_count, 1u);
  // Currently, devtools data is on the first fragment of a fragmented flexbox.
  return GetLayoutResult(0)->FlexLayoutData();
}

void LayoutFlexibleBox::RemoveChild(LayoutObject* child) {
  if (!DocumentBeingDestroyed() &&
      !StyleRef().IsDeprecatedFlexboxUsingFlexLayout())
    MergeAnonymousFlexItems(child);

  LayoutBlock::RemoveChild(child);
}

}  // namespace blink
