// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_GESTURE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_GESTURE_MANAGER_H_

#include "third_party/blink/public/common/input/pointer_id.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace gfx {
class Point;
}

namespace blink {

class LocalFrame;
class ScrollManager;
class SelectionController;
class PointerEventManager;
class MouseEventManager;
class WebPointerEvent;

// This class takes care of gestures and delegating the action based on the
// gesture to the responsible class.
class CORE_EXPORT GestureManager final
    : public GarbageCollected<GestureManager> {
 public:
  GestureManager(LocalFrame&,
                 ScrollManager&,
                 MouseEventManager&,
                 PointerEventManager&,
                 SelectionController&);
  GestureManager(const GestureManager&) = delete;
  GestureManager& operator=(const GestureManager&) = delete;
  void Trace(Visitor*) const;

  void Clear();
  void ResetLongTapContextMenuStates();

  HitTestRequest::HitTestRequestType GetHitTypeForGestureType(
      WebInputEvent::Type);
  WebInputEventResult HandleGestureEventInFrame(
      const GestureEventWithHitTestResults&);
  bool GestureContextMenuDeferred() const;

  // Dispatches contextmenu event for drag-ends that haven't really dragged
  // except for a few pixels.
  //
  // The reason for handling this in GestureManager is the similarity of the
  // interaction with long taps.  When a drag ends without a drag offset, it is
  // effectively a long tap but with one difference: there is no gesture long
  // tap event.  This is because the drag controller interrupts current gesture
  // sequence (cancelling the gesture) at the moment a drag begins, and the
  // gesture recognizer does not know if the drag has ended at the originating
  // position.
  void SendContextMenuEventTouchDragEnd(const WebMouseEvent&);

  // Gesture Manager receives notification when Pointer Events are dispatched.
  // GestureManager is interested in knowing the pointerId of pointerdown
  // event because it uses this pointer id to populate the pointerId for
  // click and auxclick pointer events it generates.
  //
  // This must be called from local root frame.
  void NotifyPointerEventHandled(const WebPointerEvent& web_pointer_event);

 private:
  WebInputEventResult HandleGestureShowPress();
  WebInputEventResult HandleGestureTapDown(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureTap(const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureShortPress(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureLongPress(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureLongTap(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureTwoFingerTap(
      const GestureEventWithHitTestResults&);

  WebInputEventResult SendContextMenuEventForGesture(
      const GestureEventWithHitTestResults&);
  // Shows the Unhandled Tap UI if needed.
  void ShowUnhandledTapUIIfNeeded(
      bool dom_tree_changed,
      bool style_changed,
      Node* tapped_node,
      const gfx::Point& tapped_position_in_viewport);
  // Returns the pointerId associated with the pointerevent sequence
  // generated by the gesture sequence of which gesture_event
  // is part of or PointerEventFactory::kInvalidId in case there is no
  // pointerId associated. This method expects that gesture_event is the
  // most recently handled WebGestureEvent.
  PointerId GetPointerIdFromWebGestureEvent(
      const WebGestureEvent& gesture_event) const;

  // Remove pointerdown id information for the events with a smaller
  // |primary_unique_touch_event_id| because all gestures prior to the given id
  // have been already handled.
  //
  // This must be called from local root frame.
  void ClearOldPointerDownIds(uint32_t primary_unique_touch_event_id);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared if needed in |GestureManager::clear()|.

  const Member<LocalFrame> frame_;

  Member<ScrollManager> scroll_manager_;
  Member<MouseEventManager> mouse_event_manager_;
  Member<PointerEventManager> pointer_event_manager_;

  // Set on GestureTapDown if the |pointerdown| event corresponding to the
  // triggering |touchstart| event was canceled. This suppresses mouse event
  // firing for the current gesture sequence (i.e. until next GestureTapDown).
  bool suppress_mouse_events_from_gestures_;

  // Set on GestureTap if the default mouse down behaviour was suppressed. When
  // this happens, we also suppress the default selection behaviour of the
  // subsequent GestureTapDown if it occurs in the same gesture sequence.
  bool suppress_selection_on_repeated_tap_down_ = true;

  bool gesture_context_menu_deferred_;

  gfx::PointF long_press_position_in_root_frame_;
  bool drag_in_progress_;

  // Pair of the unique_touch_id for the first gesture in the sequence and
  // the pointerId associated.
  using TouchIdPointerId = std::pair<uint32_t, PointerId>;
  // The mapping between unique_touch_event_id for tap down and pointer Id
  // for pointerdown. We will keep the pointerId for a pointerevents sequence
  // until we know that the pointerevents will not turn into gestures anymore.
  // We will not keep track of the mapping for unique_touch_event_id = 0
  // (unknown id) which will be the case for mouse pointer events for example.
  Deque<TouchIdPointerId> recent_pointerdown_pointer_ids_;

  const Member<SelectionController> selection_controller_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_GESTURE_MANAGER_H_
