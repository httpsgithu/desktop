// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_value_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"

namespace blink {

class SVGValueNonInterpolableValue : public NonInterpolableValue {
 public:
  ~SVGValueNonInterpolableValue() override = default;

  static scoped_refptr<SVGValueNonInterpolableValue> Create(
      SVGPropertyBase* svg_value) {
    return base::AdoptRef(new SVGValueNonInterpolableValue(svg_value));
  }

  SVGPropertyBase* SvgValue() const { return svg_value_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  SVGValueNonInterpolableValue(SVGPropertyBase* svg_value)
      : svg_value_(svg_value) {}

  Persistent<SVGPropertyBase> svg_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGValueNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGValueNonInterpolableValue);

InterpolationValue SVGValueInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& value) const {
  SVGPropertyBase* referenced_value =
      const_cast<SVGPropertyBase*>(&value);  // Take ref.
  return InterpolationValue(
      InterpolableList::Create(0),
      SVGValueNonInterpolableValue::Create(referenced_value));
}

SVGPropertyBase* SVGValueInterpolationType::AppliedSVGValue(
    const InterpolableValue&,
    const NonInterpolableValue* non_interpolable_value) const {
  return ToSVGValueNonInterpolableValue(*non_interpolable_value).SvgValue();
}

}  // namespace blink
