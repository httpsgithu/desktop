/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/Cursor.h"

namespace blink {

IntPoint determineHotSpot(Image* image,
                          bool hotSpotSpecified,
                          const IntPoint& specifiedHotSpot) {
  if (image->isNull())
    return IntPoint();

  IntRect imageRect = image->rect();

  // Hot spot must be inside cursor rectangle.
  if (hotSpotSpecified) {
    if (imageRect.contains(specifiedHotSpot)) {
      return specifiedHotSpot;
    }

    return IntPoint(
        clampTo<int>(specifiedHotSpot.x(), imageRect.x(), imageRect.maxX() - 1),
        clampTo<int>(specifiedHotSpot.y(), imageRect.y(),
                     imageRect.maxY() - 1));
  }

  // If hot spot is not specified externally, it can be extracted from some
  // image formats (e.g. .cur).
  IntPoint intrinsicHotSpot;
  bool imageHasIntrinsicHotSpot = image->getHotSpot(intrinsicHotSpot);
  if (imageHasIntrinsicHotSpot && imageRect.contains(intrinsicHotSpot))
    return intrinsicHotSpot;

  // If neither is provided, use a default value of (0, 0).
  return IntPoint();
}

Cursor::Cursor(Image* image, bool hotSpotSpecified, const IntPoint& hotSpot)
    : m_type(Custom),
      m_image(image),
      m_hotSpot(determineHotSpot(image, hotSpotSpecified, hotSpot)),
      m_imageScaleFactor(1) {}

Cursor::Cursor(Image* image,
               bool hotSpotSpecified,
               const IntPoint& hotSpot,
               float scale)
    : m_type(Custom),
      m_image(image),
      m_hotSpot(determineHotSpot(image, hotSpotSpecified, hotSpot)),
      m_imageScaleFactor(scale) {}

Cursor::Cursor(Type type) : m_type(type), m_imageScaleFactor(1) {}

Cursor::Cursor(const Cursor& other)
    : m_type(other.m_type),
      m_image(other.m_image),
      m_hotSpot(other.m_hotSpot),
      m_imageScaleFactor(other.m_imageScaleFactor) {}

Cursor& Cursor::operator=(const Cursor& other) {
  m_type = other.m_type;
  m_image = other.m_image;
  m_hotSpot = other.m_hotSpot;
  m_imageScaleFactor = other.m_imageScaleFactor;
  return *this;
}

Cursor::~Cursor() {}

const Cursor& pointerCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Pointer));
  return c;
}

const Cursor& crossCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Cross));
  return c;
}

const Cursor& handCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Hand));
  return c;
}

const Cursor& moveCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Move));
  return c;
}

const Cursor& verticalTextCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::VerticalText));
  return c;
}

const Cursor& cellCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Cell));
  return c;
}

const Cursor& contextMenuCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::ContextMenu));
  return c;
}

const Cursor& aliasCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Alias));
  return c;
}

const Cursor& zoomInCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::ZoomIn));
  return c;
}

const Cursor& zoomOutCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::ZoomOut));
  return c;
}

const Cursor& copyCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Copy));
  return c;
}

const Cursor& noneCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::None));
  return c;
}

const Cursor& progressCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Progress));
  return c;
}

const Cursor& noDropCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NoDrop));
  return c;
}

const Cursor& notAllowedCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NotAllowed));
  return c;
}

const Cursor& iBeamCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::IBeam));
  return c;
}

const Cursor& waitCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Wait));
  return c;
}

const Cursor& helpCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Help));
  return c;
}

const Cursor& eastResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::EastResize));
  return c;
}

const Cursor& northResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthResize));
  return c;
}

const Cursor& northEastResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthEastResize));
  return c;
}

const Cursor& northWestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthWestResize));
  return c;
}

const Cursor& southResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthResize));
  return c;
}

const Cursor& southEastResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthEastResize));
  return c;
}

const Cursor& southWestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthWestResize));
  return c;
}

const Cursor& westResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::WestResize));
  return c;
}

const Cursor& northSouthResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthSouthResize));
  return c;
}

const Cursor& eastWestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::EastWestResize));
  return c;
}

const Cursor& northEastSouthWestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthEastSouthWestResize));
  return c;
}

const Cursor& northWestSouthEastResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthWestSouthEastResize));
  return c;
}

const Cursor& columnResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::ColumnResize));
  return c;
}

const Cursor& rowResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::RowResize));
  return c;
}

const Cursor& middlePanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::MiddlePanning));
  return c;
}

const Cursor& eastPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::EastPanning));
  return c;
}

const Cursor& northPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthPanning));
  return c;
}

const Cursor& northEastPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthEastPanning));
  return c;
}

const Cursor& northWestPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthWestPanning));
  return c;
}

const Cursor& southPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthPanning));
  return c;
}

const Cursor& southEastPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthEastPanning));
  return c;
}

const Cursor& southWestPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthWestPanning));
  return c;
}

const Cursor& westPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::WestPanning));
  return c;
}

const Cursor& grabCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Grab));
  return c;
}

const Cursor& grabbingCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Grabbing));
  return c;
}

}  // namespace blink
