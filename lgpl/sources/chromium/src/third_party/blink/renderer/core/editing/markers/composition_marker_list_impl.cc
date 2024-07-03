// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/composition_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/overlapping_document_marker_list_editor.h"
#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"

namespace blink {

DocumentMarker::MarkerType CompositionMarkerListImpl::MarkerType() const {
  return DocumentMarker::kComposition;
}

bool CompositionMarkerListImpl::IsEmpty() const {
  return markers_.empty();
}

void CompositionMarkerListImpl::Add(DocumentMarker* marker) {
  DCHECK_EQ(DocumentMarker::kComposition, marker->GetType());
  OverlappingDocumentMarkerListEditor::AddMarker(&markers_, marker);
}

void CompositionMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>&
CompositionMarkerListImpl::GetMarkers() const {
  return markers_;
}

DocumentMarker* CompositionMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
CompositionMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                    unsigned end_offset) const {
  return OverlappingDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool CompositionMarkerListImpl::MoveMarkers(int length,
                                            DocumentMarkerList* dst_markers_) {
  return OverlappingDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                          dst_markers_);
}

bool CompositionMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                              int length) {
  return OverlappingDocumentMarkerListEditor::RemoveMarkers(
      &markers_, start_offset, length);
}

bool CompositionMarkerListImpl::ShiftMarkers(const String&,
                                             unsigned offset,
                                             unsigned old_length,
                                             unsigned new_length) {
  return OverlappingDocumentMarkerListEditor::ShiftMarkers(
      &markers_, offset, old_length, new_length);
}

void CompositionMarkerListImpl::Trace(Visitor* visitor) const {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
