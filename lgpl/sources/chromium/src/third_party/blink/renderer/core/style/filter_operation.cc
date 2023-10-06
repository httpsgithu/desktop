/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/style/filter_operation.h"

#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_drop_shadow.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_gaussian_blur.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"

#if BUILDFLAG(OPERA_FEATURE_BLINK_GPU_SHADER_CSS_FILTER)
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/style/gpu_shader_resource.h"
#endif  // BUILDFLAG(OPERA_FEATURE_BLINK_GPU_SHADER_CSS_FILTER)

namespace blink {

void ReferenceFilterOperation::Trace(Visitor* visitor) const {
  visitor->Trace(resource_);
  visitor->Trace(filter_);
  FilterOperation::Trace(visitor);
}

gfx::RectF ReferenceFilterOperation::MapRect(const gfx::RectF& rect) const {
  const auto* last_effect = filter_ ? filter_->LastEffect() : nullptr;
  if (!last_effect) {
    return rect;
  }
  return last_effect->MapRect(rect);
}

ReferenceFilterOperation::ReferenceFilterOperation(const AtomicString& url,
                                                   SVGResource* resource)
    : FilterOperation(OperationType::kReference),
      url_(url),
      resource_(resource) {}

void ReferenceFilterOperation::AddClient(SVGResourceClient& client) {
  if (resource_) {
    resource_->AddClient(client);
  }
}

void ReferenceFilterOperation::RemoveClient(SVGResourceClient& client) {
  if (resource_) {
    resource_->RemoveClient(client);
  }
}

bool ReferenceFilterOperation::IsEqualAssumingSameType(
    const FilterOperation& o) const {
  const auto& other = To<ReferenceFilterOperation>(o);
  return url_ == other.url_ && resource_ == other.resource_;
}

gfx::RectF BlurFilterOperation::MapRect(const gfx::RectF& rect) const {
  float std_deviation = FloatValueForLength(std_deviation_, 0);
  return FEGaussianBlur::MapEffect(gfx::SizeF(std_deviation, std_deviation),
                                   rect);
}

gfx::RectF DropShadowFilterOperation::MapRect(const gfx::RectF& rect) const {
  float std_deviation = shadow_.Blur();
  return FEDropShadow::MapEffect(gfx::SizeF(std_deviation, std_deviation),
                                 shadow_.Offset(), rect);
}

gfx::RectF BoxReflectFilterOperation::MapRect(const gfx::RectF& rect) const {
  return reflection_.MapRect(rect);
}

bool BoxReflectFilterOperation::IsEqualAssumingSameType(
    const FilterOperation& o) const {
  const auto& other = static_cast<const BoxReflectFilterOperation&>(o);
  return reflection_ == other.reflection_;
}

#if BUILDFLAG(OPERA_FEATURE_BLINK_GPU_SHADER_CSS_FILTER)
GpuShaderFilterOperation::GpuShaderFilterOperation(
    const AtomicString& relative_url,
    const AtomicString& absolute_url,
    const Referrer& referrer,
    GpuShaderResource* resource,
    const CSSValueList* args,
    float animation_frame)
    : FilterOperation(OperationType::kGpuShader),
      relative_url_(relative_url),
      absolute_url_(absolute_url),
      referrer_(referrer),
      resource_(resource),
      args_(args),
      animation_frame_(animation_frame) {
  DCHECK(args);
}

gfx::RectF GpuShaderFilterOperation::MapRect(const gfx::RectF& rect) const {
  return rect;
}

void GpuShaderFilterOperation::AddClient(GpuShaderResourceClient& client) {
  if (resource_)
    resource_->AddClient(client);
}

void GpuShaderFilterOperation::RemoveClient(GpuShaderResourceClient& client) {
  if (resource_)
    resource_->RemoveClient(client);
}

bool GpuShaderFilterOperation::IsEqualAssumingSameType(
    const FilterOperation& o) const {
  const auto& other = To<GpuShaderFilterOperation>(o);
  return relative_url_ == other.relative_url_ &&
         absolute_url_ == other.absolute_url_ && resource_ == other.resource_ &&
         args_ == other.args_ && animation_frame_ == other.animation_frame_;
}

void GpuShaderFilterOperation::Trace(Visitor* visitor) const {
  visitor->Trace(args_);
  visitor->Trace(resource_);
  FilterOperation::Trace(visitor);
}
#endif  // BUILDFLAG(OPERA_FEATURE_BLINK_GPU_SHADER_CSS_FILTER)

}  // namespace blink
