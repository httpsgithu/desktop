// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/style/inset_area.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AnchorQuery;
class AnchorScope;
class ComputedStyleBuilder;
class ScopedCSSName;

class CORE_EXPORT AnchorEvaluator {
  DISALLOW_NEW();

 public:
  AnchorEvaluator() = default;

  // The evaluation of anchor() and anchor-size() functions is affected
  // by the context they are used in. For example, it is not allowed to
  // do anchor() queries "cross-axis" (e.g. left:anchor(--a top)),
  // and anchor-size() queries are only valid in sizing properties.
  // Queries that violate these rules instead resolve to their fallback
  // values (or 0px if no fallback value exists).
  //
  // The default mode of AnchorEvaluator (kNone) is to return nullopt (i.e.
  // fallback) for any query. This represents a context where no anchor query
  // is valid, e.g. a property unrelated to insets or sizing.
  //
  // The values kLeft, kRight, kTop and kBottom represent the corresponding
  // inset properties, and allow anchor() queries [1] (with restrictions),
  // but not anchor-size() queries.
  //
  // The value kSize represents supported sizing properties [2], and allows
  // anchor-size(), but not anchor().
  //
  // The current mode can be set by placing an AnchorScope object on the
  // stack.
  //
  // [1] https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
  // [2] https://drafts.csswg.org/css-anchor-position-1/#anchor-size-valid
  enum class Mode {
    kNone,

    // anchor()
    kLeft,
    kRight,
    kTop,
    kBottom,

    // anchor() functions used for computing inset-area offsets before
    // inset-area is modifying the containing block size. These are kept
    // separately from the explicit anchor() functions for caching purposes in
    // AnchorResults because anchor(left) yield a different result depending on
    // whether the inset-area has modified the containing block size or not.
    kBaseLeft,
    kBaseRight,
    kBaseTop,
    kBaseBottom,

    // anchor-size()
    kSize
  };

  static bool IsBaseMode(AnchorEvaluator::Mode mode) {
    switch (mode) {
      case Mode::kBaseLeft:
      case Mode::kBaseRight:
      case Mode::kBaseTop:
      case Mode::kBaseBottom:
        return true;
      default:
        return false;
    }
  }

  // Evaluates an anchor() or anchor-size() query.
  // Returns |nullopt| if the query is invalid (e.g., no targets or wrong
  // axis.), in which case the fallback should be used.
  virtual std::optional<LayoutUnit> Evaluate(
      const AnchorQuery&,
      const ScopedCSSName* position_anchor,
      const std::optional<InsetAreaOffsets>&) = 0;

  // Take the computed inset-area and position-anchor and compute the physical
  // offsets to inset the containing block with.
  virtual std::optional<InsetAreaOffsets> ComputeInsetAreaOffsetsForLayout(
      const ScopedCSSName* position_anchor,
      InsetArea inset_area) = 0;

  // Take the computed inset-area and position-anchor from the builder and
  // compute the physical offset for anchor-center
  virtual std::optional<PhysicalOffset> ComputeAnchorCenterOffsets(
      const ComputedStyleBuilder&) = 0;

  virtual void Trace(Visitor*) const {}

 protected:
  Mode GetMode() const { return mode_; }

 private:
  friend class AnchorScope;

  // The computed position-anchor in use for the current try option.
  Mode mode_ = Mode::kNone;
};

// Temporarily sets the Mode of an AnchorEvaluator.
//
// This class behaves like base::AutoReset, except it allows `anchor_evalutor`
// to be nullptr (in which case the AnchorScope has no effect).
//
// See AnchorEvaluator::Mode for more information.
class CORE_EXPORT AnchorScope {
  STACK_ALLOCATED();

 public:
  using Mode = AnchorEvaluator::Mode;

  explicit AnchorScope(Mode mode, AnchorEvaluator* anchor_evaluator)
      : target_(anchor_evaluator ? &anchor_evaluator->mode_ : nullptr),
        original_(anchor_evaluator ? anchor_evaluator->mode_ : Mode::kNone) {
    if (target_) {
      *target_ = mode;
    }
  }
  ~AnchorScope() {
    if (target_) {
      *target_ = original_;
    }
  }

 private:
  Mode* target_;
  Mode original_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_
