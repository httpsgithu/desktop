/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"

#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/svg_foreign_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"

namespace blink {

LayoutSVGForeignObject::LayoutSVGForeignObject(SVGForeignObjectElement* node)
    : LayoutSVGBlock(node), needs_transform_update_(true) {}

LayoutSVGForeignObject::~LayoutSVGForeignObject() = default;

bool LayoutSVGForeignObject::IsChildAllowed(LayoutObject* child,
                                            const ComputedStyle& style) const {
  // Disallow arbitary SVG content. Only allow proper <svg xmlns="svgNS">
  // subdocuments.
  return !child->IsSVGChild();
}

void LayoutSVGForeignObject::Paint(const PaintInfo& paint_info) const {
  SVGForeignObjectPainter(*this).Paint(paint_info);
}

LayoutUnit LayoutSVGForeignObject::ElementX() const {
  return LayoutUnit(
      roundf(SVGLengthContext(GetElement())
                 .ValueForLength(StyleRef().SvgStyle().X(), StyleRef(),
                                 SVGLengthMode::kWidth)));
}

LayoutUnit LayoutSVGForeignObject::ElementY() const {
  return LayoutUnit(
      roundf(SVGLengthContext(GetElement())
                 .ValueForLength(StyleRef().SvgStyle().Y(), StyleRef(),
                                 SVGLengthMode::kHeight)));
}

LayoutUnit LayoutSVGForeignObject::ElementWidth() const {
  return LayoutUnit(SVGLengthContext(GetElement())
                        .ValueForLength(StyleRef().Width(), StyleRef(),
                                        SVGLengthMode::kWidth));
}

LayoutUnit LayoutSVGForeignObject::ElementHeight() const {
  return LayoutUnit(SVGLengthContext(GetElement())
                        .ValueForLength(StyleRef().Height(), StyleRef(),
                                        SVGLengthMode::kHeight));
}

void LayoutSVGForeignObject::UpdateLogicalWidth() {
  SetLogicalWidth(StyleRef().IsHorizontalWritingMode() ? ElementWidth()
                                                       : ElementHeight());
}

void LayoutSVGForeignObject::ComputeLogicalHeight(
    LayoutUnit,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  computed_values.extent_ =
      StyleRef().IsHorizontalWritingMode() ? ElementHeight() : ElementWidth();
  computed_values.position_ = logical_top;
}

void LayoutSVGForeignObject::UpdateLayout() {
  DCHECK(NeedsLayout());

  SVGForeignObjectElement* foreign = ToSVGForeignObjectElement(GetElement());

  bool update_cached_boundaries_in_parents = false;
  if (needs_transform_update_) {
    local_transform_ =
        foreign->CalculateTransform(SVGElement::kIncludeMotionTransform);
    needs_transform_update_ = false;
    update_cached_boundaries_in_parents = true;
  }

  LayoutRect old_viewport = FrameRect();

  // Set box origin to the foreignObject x/y translation, so positioned objects
  // in XHTML content get correct positions. A regular LayoutBoxModelObject
  // would pull this information from ComputedStyle - in SVG those properties
  // are ignored for non <svg> elements, so we mimic what happens when
  // specifying them through CSS.
  SetX(ElementX());
  SetY(ElementY());

  bool layout_changed = EverHadLayout() && SelfNeedsLayout();
  LayoutBlock::UpdateLayout();
  DCHECK(!NeedsLayout());

  // If our bounds changed, notify the parents.
  if (!update_cached_boundaries_in_parents)
    update_cached_boundaries_in_parents = old_viewport != FrameRect();
  if (update_cached_boundaries_in_parents)
    LayoutSVGBlock::SetNeedsBoundariesUpdate();

  // Invalidate all resources of this client if our layout changed.
  if (layout_changed)
    SVGResourcesCache::ClientLayoutChanged(*this);
}

bool LayoutSVGForeignObject::NodeAtPointFromSVG(
    HitTestResult& result,
    const HitTestLocation& location_in_parent,
    const LayoutPoint& accumulated_offset,
    HitTestAction) {
  DCHECK_EQ(accumulated_offset, LayoutPoint());
  AffineTransform local_transform = LocalSVGTransform();
  if (!local_transform.IsInvertible())
    return false;

  AffineTransform inverse = local_transform.Inverse();
  base::Optional<HitTestLocation> local_location;
  if (location_in_parent.IsRectBasedTest()) {
    local_location.emplace(
        inverse.MapPoint(location_in_parent.TransformedPoint()),
        inverse.MapQuad(location_in_parent.TransformedRect()));
  } else {
    local_location.emplace(
        (inverse.MapPoint(location_in_parent.TransformedPoint())));
  }

  // |local_point| already includes the offset of the <foreignObject> element,
  // but PaintLayer::HitTestLayer assumes it has not been.
  HitTestLocation local_without_offset(
      *local_location, -ToLayoutSize(Layer()->LayoutBoxLocation()));
  HitTestResult layer_result(result.GetHitTestRequest(), local_without_offset);
  bool retval = Layer()->HitTest(local_without_offset, layer_result,
                                 LayoutRect(LayoutRect::InfiniteIntRect()));

  // Preserve the "point in inner node frame" from the original request,
  // since |layer_result| is a hit test rooted at the <foreignObject> element,
  // not the frame, due to the constructor above using
  // |point_in_foreign_object| as its "point in inner node frame".
  // TODO(chrishtr): refactor the PaintLayer and HitTestResults code around
  // this, to better support hit tests that don't start at frame boundaries.
  LayoutPoint original_point_in_inner_node_frame =
      result.PointInInnerNodeFrame();
  if (result.GetHitTestRequest().ListBased())
    result.Append(layer_result);
  else
    result = layer_result;
  result.SetPointInInnerNodeFrame(original_point_in_inner_node_frame);
  return retval;
}

bool LayoutSVGForeignObject::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_parent,
    const LayoutPoint& accumulated_offset,
    HitTestAction hit_test_action) {
  // Skip LayoutSVGBlock's override.
  return LayoutBlockFlow::NodeAtPoint(result, location_in_parent,
                                      accumulated_offset, hit_test_action);
}

PaintLayerType LayoutSVGForeignObject::LayerTypeRequired() const {
  // Skip LayoutSVGBlock's override.
  return LayoutBlockFlow::LayerTypeRequired();
}

void LayoutSVGForeignObject::StyleDidChange(StyleDifference diff,
                                            const ComputedStyle* old_style) {
  LayoutSVGBlock::StyleDidChange(diff, old_style);

  if (old_style && (SVGLayoutSupport::IsOverflowHidden(*old_style) !=
                    SVGLayoutSupport::IsOverflowHidden(StyleRef()))) {
    // See NeedsOverflowClip() in PaintPropertyTreeBuilder for the reason.
    SetNeedsPaintPropertyUpdate();

    if (Layer())
      Layer()->SetNeedsCompositingInputsUpdate();
  }
}

}  // namespace blink
