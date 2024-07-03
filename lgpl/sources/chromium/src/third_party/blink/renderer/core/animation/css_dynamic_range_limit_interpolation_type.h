// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_DYNAMIC_RANGE_LIMIT_INTERPOLATION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_DYNAMIC_RANGE_LIMIT_INTERPOLATION_TYPE_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"

namespace blink {

class CORE_EXPORT CSSDynamicRangeLimitInterpolationType
    : public CSSInterpolationType {
 public:
  explicit CSSDynamicRangeLimitInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {
    DCHECK(property.GetCSSProperty().PropertyID() ==
           CSSPropertyID::kDynamicRangeLimit);
  }
  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;
  static InterpolationValue ConvertDynamicRangeLimit(DynamicRangeLimit limit);
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_DYNAMIC_RANGE_LIMIT_INTERPOLATION_TYPE_H_
