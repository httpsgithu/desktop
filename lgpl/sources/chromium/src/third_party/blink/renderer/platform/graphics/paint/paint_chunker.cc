// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

namespace blink {

static const PropertyTreeState& UninitializedProperties() {
  DEFINE_STATIC_LOCAL(PropertyTreeState, initial_properties,
                      (nullptr, nullptr, nullptr));
  return initial_properties;
}

PaintChunker::PaintChunker()
    : current_properties_(UninitializedProperties()), force_new_chunk_(false) {}

PaintChunker::~PaintChunker() = default;

bool PaintChunker::IsInInitialState() const {
  if (current_properties_ != UninitializedProperties())
    return false;

  DCHECK(chunks_.IsEmpty());
  return true;
}

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const base::Optional<PaintChunk::Id>& chunk_id,
    const PropertyTreeState& properties) {
  // If properties are the same, continue to use the previously set
  // |next_chunk_id_| because the id of the outer painting is likely to be
  // more stable to reduce invalidation because of chunk id changes.
  if (!next_chunk_id_ || current_properties_ != properties) {
    if (chunk_id)
      next_chunk_id_.emplace(*chunk_id);
    else
      next_chunk_id_ = base::nullopt;
  }
  current_properties_ = properties;
}

void PaintChunker::ForceNewChunk() {
  force_new_chunk_ = true;
  // Always use a new chunk id for a force chunk which may be for a subsequence
  // which needs the chunk id to be independence with previous chunks.
  next_chunk_id_ = base::nullopt;
}

bool PaintChunker::IncrementDisplayItemIndex(const DisplayItem& item) {
  // Property nodes should never be null because they should either be set to
  // properties created by a LayoutObject/FrameView, or be set to a non-null
  // root node. If these DCHECKs are hit we are missing a call to update the
  // properties. See: ScopedPaintChunkProperties.
  DCHECK(current_properties_.Transform());
  DCHECK(current_properties_.Clip());
  DCHECK(current_properties_.Effect());

  bool item_forces_new_chunk = item.IsForeignLayer() || item.IsScrollHitTest();
  if (item_forces_new_chunk)
    force_new_chunk_ = true;

  size_t new_chunk_begin_index;
  if (chunks_.IsEmpty()) {
    new_chunk_begin_index = 0;
  } else {
    auto& last_chunk = LastChunk();
    if (!force_new_chunk_ && current_properties_ == last_chunk.properties) {
      // Continue the current chunk.
      last_chunk.end_index++;
      // We don't create a new chunk when UpdateCurrentPaintChunkProperties()
      // just changed |next_chunk_id_| but not |current_properties_|. Clear
      // |next_chunk_id_| which has been ignored.
      next_chunk_id_ = base::nullopt;
      return false;
    }
    new_chunk_begin_index = last_chunk.end_index;
  }

  chunks_.emplace_back(new_chunk_begin_index, new_chunk_begin_index + 1,
                       next_chunk_id_ ? *next_chunk_id_ : item.GetId(),
                       current_properties_);
  next_chunk_id_ = base::nullopt;

  // When forcing a new chunk, we still need to force new chunk for the next
  // display item. Otherwise reset force_new_chunk_ to false.
  if (!item_forces_new_chunk)
    force_new_chunk_ = false;

  return true;
}

Vector<PaintChunk> PaintChunker::ReleasePaintChunks() {
  next_chunk_id_ = base::nullopt;
  current_properties_ = UninitializedProperties();
  chunks_.ShrinkToFit();
  return std::move(chunks_);
}

}  // namespace blink
