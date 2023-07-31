// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_SIZE_ADJUST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_SIZE_ADJUST_H_

#include "third_party/blink/renderer/platform/platform_export.h"

namespace WTF {
class String;
}

namespace blink {

class PLATFORM_EXPORT FontSizeAdjust {
 public:
  enum class Metric { kExHeight, kCapHeight, kChWidth, kIcWidth };

  FontSizeAdjust() = default;
  explicit FontSizeAdjust(float value) : value_(value) {}
  explicit FontSizeAdjust(float value, Metric metric)
      : value_(value), metric_(metric) {}

  static constexpr float kFontSizeAdjustNone = -1;

  explicit operator bool() const { return value_ != kFontSizeAdjustNone; }
  bool operator==(const FontSizeAdjust& other) const {
    return value_ == other.Value() && metric_ == other.GetMetric();
  }
  bool operator!=(const FontSizeAdjust& other) const {
    return !operator==(other);
  }

  float Value() const { return value_; }
  Metric GetMetric() const { return metric_; }

  unsigned GetHash() const;
  WTF::String ToString() const;

 private:
  WTF::String ToString(Metric metric) const;

  float value_{kFontSizeAdjustNone};
  Metric metric_{Metric::kExHeight};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_SIZE_ADJUST_H_
