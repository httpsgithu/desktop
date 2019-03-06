/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter_primitive.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"

namespace blink {

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(
    const QualifiedName& tag_name,
    Document& document)
    : SVGElement(tag_name, document),
      // Spec: If the x/y attribute is not specified, the effect is as if a
      // value of "0%" were specified.
      x_(SVGAnimatedLength::Create(this,
                                   SVGNames::xAttr,
                                   SVGLengthMode::kWidth,
                                   SVGLength::Initial::kPercent0)),
      y_(SVGAnimatedLength::Create(this,
                                   SVGNames::yAttr,
                                   SVGLengthMode::kHeight,
                                   SVGLength::Initial::kPercent0)),
      // Spec: If the width/height attribute is not specified, the effect is as
      // if a value of "100%" were specified.
      width_(SVGAnimatedLength::Create(this,
                                       SVGNames::widthAttr,
                                       SVGLengthMode::kWidth,
                                       SVGLength::Initial::kPercent100)),
      height_(SVGAnimatedLength::Create(this,
                                        SVGNames::heightAttr,
                                        SVGLengthMode::kHeight,
                                        SVGLength::Initial::kPercent100)),
      result_(SVGAnimatedString::Create(this, SVGNames::resultAttr)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
  AddToPropertyMap(result_);
}

void SVGFilterPrimitiveStandardAttributes::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(result_);
  SVGElement::Trace(visitor);
}

bool SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  DCHECK(attr_name == SVGNames::color_interpolation_filtersAttr);
  DCHECK(GetLayoutObject());
  EColorInterpolation color_interpolation =
      GetLayoutObject()->StyleRef().SvgStyle().ColorInterpolationFilters();
  InterpolationSpace resolved_interpolation_space =
      SVGFilterBuilder::ResolveInterpolationSpace(color_interpolation);
  if (resolved_interpolation_space == effect->OperatingInterpolationSpace())
    return false;
  effect->SetOperatingInterpolationSpace(resolved_interpolation_space);
  return true;
}

void SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == SVGNames::xAttr || attr_name == SVGNames::yAttr ||
      attr_name == SVGNames::widthAttr || attr_name == SVGNames::heightAttr ||
      attr_name == SVGNames::resultAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

void SVGFilterPrimitiveStandardAttributes::ChildrenChanged(
    const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (!change.by_parser)
    Invalidate();
}

static FloatRect DefaultFilterPrimitiveSubregion(FilterEffect* filter_effect) {
  // https://drafts.fxtf.org/filter-effects/#FilterPrimitiveSubRegion
  DCHECK(filter_effect->GetFilter());

  // <feTurbulence>, <feFlood> and <feImage> don't have input effects, so use
  // the filter region as default subregion. <feTile> does have an input
  // reference, but due to its function (and special-cases) its default
  // resolves to the filter region.
  if (filter_effect->GetFilterEffectType() == kFilterEffectTypeTile ||
      !filter_effect->NumberOfEffectInputs())
    return filter_effect->GetFilter()->FilterRegion();

  // "x, y, width and height default to the union (i.e., tightest fitting
  // bounding box) of the subregions defined for all referenced nodes."
  FloatRect subregion_union;
  for (const auto& input_effect : filter_effect->InputEffects()) {
    // "If ... one or more of the referenced nodes is a standard input
    // ... the default subregion is 0%, 0%, 100%, 100%, where as a
    // special-case the percentages are relative to the dimensions of the
    // filter region..."
    if (input_effect->GetFilterEffectType() == kFilterEffectTypeSourceInput)
      return filter_effect->GetFilter()->FilterRegion();
    subregion_union.Unite(input_effect->FilterPrimitiveSubregion());
  }
  return subregion_union;
}

void SVGFilterPrimitiveStandardAttributes::SetStandardAttributes(
    FilterEffect* filter_effect,
    SVGUnitTypes::SVGUnitType primitive_units,
    const FloatRect& reference_box) const {
  DCHECK(filter_effect);

  FloatRect subregion = DefaultFilterPrimitiveSubregion(filter_effect);
  FloatRect primitive_boundaries =
      SVGLengthContext::ResolveRectangle(this, primitive_units, reference_box);

  if (x()->IsSpecified())
    subregion.SetX(primitive_boundaries.X());
  if (y()->IsSpecified())
    subregion.SetY(primitive_boundaries.Y());
  if (width()->IsSpecified())
    subregion.SetWidth(primitive_boundaries.Width());
  if (height()->IsSpecified())
    subregion.SetHeight(primitive_boundaries.Height());

  filter_effect->SetFilterPrimitiveSubregion(subregion);
}

LayoutObject* SVGFilterPrimitiveStandardAttributes::CreateLayoutObject(
    const ComputedStyle&) {
  return new LayoutSVGResourceFilterPrimitive(this);
}

bool SVGFilterPrimitiveStandardAttributes::LayoutObjectIsNeeded(
    const ComputedStyle& style) const {
  if (IsSVGFilterElement(parentNode()))
    return SVGElement::LayoutObjectIsNeeded(style);

  return false;
}

void SVGFilterPrimitiveStandardAttributes::Invalidate() {
  if (SVGFilterElement* filter = ToSVGFilterElementOrNull(parentElement()))
    filter->InvalidateFilterChain();
}

void SVGFilterPrimitiveStandardAttributes::PrimitiveAttributeChanged(
    const QualifiedName& attribute) {
  if (SVGFilterElement* filter = ToSVGFilterElementOrNull(parentElement()))
    filter->PrimitiveAttributeChanged(*this, attribute);
}

void InvalidateFilterPrimitiveParent(SVGElement& element) {
  Element* parent = element.parentElement();
  if (!parent || !parent->IsSVGElement())
    return;
  SVGElement& svgparent = ToSVGElement(*parent);
  if (!IsSVGFilterPrimitiveStandardAttributes(svgparent))
    return;
  ToSVGFilterPrimitiveStandardAttributes(svgparent).Invalidate();
}

}  // namespace blink
