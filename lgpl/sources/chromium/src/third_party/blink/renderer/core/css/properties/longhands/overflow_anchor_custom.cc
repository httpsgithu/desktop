// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/overflow_anchor.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* OverflowAnchor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverflowAnchor());
}

}  // namespace CSSLonghand
}  // namespace blink
