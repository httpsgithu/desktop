/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_fe_flood_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/svg_computed_style.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_flood.h"

namespace blink {

inline SVGFEFloodElement::SVGFEFloodElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feFloodTag, document) {}

DEFINE_NODE_FACTORY(SVGFEFloodElement)

bool SVGFEFloodElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  const ComputedStyle& style = ComputedStyleRef();

  FEFlood* flood = static_cast<FEFlood*>(effect);
  if (attr_name == SVGNames::flood_colorAttr) {
    return flood->SetFloodColor(
        style.VisitedDependentColor(GetCSSPropertyFloodColor()));
  }
  if (attr_name == SVGNames::flood_opacityAttr)
    return flood->SetFloodOpacity(style.SvgStyle().FloodOpacity());

  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

FilterEffect* SVGFEFloodElement::Build(SVGFilterBuilder*, Filter* filter) {
  const ComputedStyle* style = GetComputedStyle();
  if (!style)
    return nullptr;

  Color color = style->VisitedDependentColor(GetCSSPropertyFloodColor());
  float opacity = style->SvgStyle().FloodOpacity();

  return FEFlood::Create(filter, color, opacity);
}

}  // namespace blink
