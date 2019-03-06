/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_mask_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"

namespace blink {

inline SVGMaskElement::SVGMaskElement(Document& document)
    : SVGElement(SVGNames::maskTag, document),
      SVGTests(this),
      // Spec: If the x/y attribute is not specified, the effect is as if a
      // value of "-10%" were specified.
      x_(SVGAnimatedLength::Create(this,
                                   SVGNames::xAttr,
                                   SVGLengthMode::kWidth,
                                   SVGLength::Initial::kPercentMinus10,
                                   CSSPropertyX)),
      y_(SVGAnimatedLength::Create(this,
                                   SVGNames::yAttr,
                                   SVGLengthMode::kHeight,
                                   SVGLength::Initial::kPercentMinus10,
                                   CSSPropertyY)),
      // Spec: If the width/height attribute is not specified, the effect is as
      // if a value of "120%" were specified.
      width_(SVGAnimatedLength::Create(this,
                                       SVGNames::widthAttr,
                                       SVGLengthMode::kWidth,
                                       SVGLength::Initial::kPercent120,
                                       CSSPropertyWidth)),
      height_(SVGAnimatedLength::Create(this,
                                        SVGNames::heightAttr,
                                        SVGLengthMode::kHeight,
                                        SVGLength::Initial::kPercent120,
                                        CSSPropertyHeight)),
      mask_units_(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::Create(
          this,
          SVGNames::maskUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)),
      mask_content_units_(
          SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::Create(
              this,
              SVGNames::maskContentUnitsAttr,
              SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
  AddToPropertyMap(mask_units_);
  AddToPropertyMap(mask_content_units_);
}

void SVGMaskElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(mask_units_);
  visitor->Trace(mask_content_units_);
  SVGElement::Trace(visitor);
  SVGTests::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGMaskElement)

void SVGMaskElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == x_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            x_->CssValue());
  } else if (property == y_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            y_->CssValue());
  } else if (property == width_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            width_->CssValue());
  } else if (property == height_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            height_->CssValue());
  } else {
    SVGElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void SVGMaskElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  bool is_length_attr =
      attr_name == SVGNames::xAttr || attr_name == SVGNames::yAttr ||
      attr_name == SVGNames::widthAttr || attr_name == SVGNames::heightAttr;

  if (is_length_attr || attr_name == SVGNames::maskUnitsAttr ||
      attr_name == SVGNames::maskContentUnitsAttr ||
      SVGTests::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    if (is_length_attr) {
      InvalidateSVGPresentationAttributeStyle();
      SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::FromAttribute(attr_name));
      UpdateRelativeLengthsInformation();
    }

    LayoutSVGResourceContainer* layout_object =
        ToLayoutSVGResourceContainer(this->GetLayoutObject());
    if (layout_object)
      layout_object->InvalidateCacheAndMarkForLayout();

    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

void SVGMaskElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (change.by_parser)
    return;

  if (LayoutObject* object = GetLayoutObject())
    object->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kChildChanged);
}

LayoutObject* SVGMaskElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGResourceMasker(this);
}

bool SVGMaskElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

}  // namespace blink
