// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_path_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/path_interpolation_functions.h"

#include "third_party/blink/renderer/core/svg/svg_path.h"

namespace blink {

InterpolationValue SVGPathInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedPath)
    return nullptr;

  return PathInterpolationFunctions::ConvertValue(
      ToSVGPath(svg_value).ByteStream(),
      PathInterpolationFunctions::PreserveCoordinates);
}

InterpolationValue SVGPathInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return PathInterpolationFunctions::MaybeConvertNeutral(underlying,
                                                         conversion_checkers);
}

PairwiseInterpolationValue SVGPathInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return PathInterpolationFunctions::MaybeMergeSingles(std::move(start),
                                                       std::move(end));
}

void SVGPathInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  PathInterpolationFunctions::Composite(underlying_value_owner,
                                        underlying_fraction, *this, value);
}

SVGPropertyBase* SVGPathInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) const {
  return SVGPath::Create(
      cssvalue::CSSPathValue::Create(PathInterpolationFunctions::AppliedValue(
          interpolable_value, non_interpolable_value)));
}

}  // namespace blink
