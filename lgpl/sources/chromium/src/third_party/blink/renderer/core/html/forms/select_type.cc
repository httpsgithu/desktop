/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/select_type.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/menu_list_inner_element.h"
#include "third_party/blink/renderer/core/html/forms/popup_menu.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/base/ui_base_features.h"

namespace blink {

class PopupUpdater;

namespace {

HTMLOptionElement* EventTargetOption(const Event& event) {
  return DynamicTo<HTMLOptionElement>(event.target()->ToNode());
}

bool CanAssignToSelectSlot(const Node& node) {
  // Even if options/optgroups are not rendered as children of menulist SELECT,
  // we still need to add them to the flat tree through slotting since we need
  // their ComputedStyle for popup rendering.
  return node.HasTagName(html_names::kOptionTag) ||
         node.HasTagName(html_names::kOptgroupTag) ||
         node.HasTagName(html_names::kHrTag);
}

}  // anonymous namespace

// TODO(crbug.com/1511354): Rename this class to PopUpSelectType
class MenuListSelectType final : public SelectType {
 public:
  explicit MenuListSelectType(HTMLSelectElement& select) : SelectType(select) {}
  void Trace(Visitor* visitor) const override;

  bool DefaultEventHandler(const Event& event) override;
  void DidSelectOption(HTMLOptionElement* element,
                       HTMLSelectElement::SelectOptionFlags flags,
                       bool should_update_popup) override;
  void DidBlur() override;
  void DidDetachLayoutTree() override;
  void DidRecalcStyle(const StyleRecalcChange change) override;
  void DidSetSuggestedOption(HTMLOptionElement* option) override;
  void SaveLastSelection() override;

  void UpdateTextStyle() override { UpdateTextStyleInternal(); }
  void UpdateTextStyleAndContent() override;
  HTMLOptionElement* OptionToBeShown() const override;
  const ComputedStyle* OptionStyle() const override {
    return option_style_.Get();
  }
  void MaximumOptionWidthMightBeChanged() const override;

  void CreateShadowSubtree(ShadowRoot& root) override;
  void ManuallyAssignSlots() override;
  HTMLButtonElement* SlottedButton() const override;
  HTMLDataListElement* DisplayedDatalist() const override;
  bool IsAppearanceBaseSelect() const override;
  Element& InnerElementForAppearanceAuto() const override;
  void ShowPopup(PopupMenu::ShowEventType type) override;
  void HidePopup() override;
  void PopupDidHide() override;
  bool PopupIsVisible() const override;
  PopupMenu* PopupForTesting() const override;
  AXObject* PopupRootAXObject() const override;
  void ShowPicker() override;

  void DidMutateSubtree();

 private:
  bool ShouldOpenPopupForKeyDownEvent(const KeyboardEvent& event);
  bool ShouldOpenPopupForKeyPressEvent(const KeyboardEvent& event);
  // Returns true if this function handled the event.
  bool HandlePopupOpenKeyboardEvent();
  void SetNativePopupIsVisible(bool popup_is_visible);
  void DispatchEventsIfSelectedOptionChanged();
  String UpdateTextStyleInternal();
  void DidUpdateActiveOption(HTMLOptionElement* option);
  void ObserveTreeMutation();
  void UnobserveTreeMutation();

  Member<PopupMenu> popup_;
  Member<PopupUpdater> popup_updater_;
  Member<const ComputedStyle> option_style_;
  Member<HTMLSlotElement> button_slot_;
  Member<HTMLButtonElement> default_button_;
  Member<HTMLDataListElement> default_datalist_;
  Member<HTMLSlotElement> default_datalist_options_slot_;
  Member<HTMLSlotElement> datalist_slot_;
  Member<HTMLSlotElement> option_slot_;
  Member<MenuListInnerElement> inner_element_;
  int ax_menulist_last_active_index_ = -1;
  bool has_updated_menulist_active_option_ = false;
  bool native_popup_is_visible_ = false;
  bool snav_arrow_key_selection_ = false;
  bool is_appearance_base_select_ = false;
};

void MenuListSelectType::Trace(Visitor* visitor) const {
  visitor->Trace(popup_);
  visitor->Trace(popup_updater_);
  visitor->Trace(option_style_);
  visitor->Trace(button_slot_);
  visitor->Trace(default_button_);
  visitor->Trace(default_datalist_);
  visitor->Trace(default_datalist_options_slot_);
  visitor->Trace(datalist_slot_);
  visitor->Trace(option_slot_);
  visitor->Trace(inner_element_);
  SelectType::Trace(visitor);
}

bool MenuListSelectType::DefaultEventHandler(const Event& event) {
  // We need to make the layout tree up-to-date to have GetLayoutObject() give
  // the correct result below. An author event handler may have set display to
  // some element to none which will cause a layout tree detach.
  select_->GetDocument().UpdateStyleAndLayoutTree();

  const int ignore_modifiers = WebInputEvent::kShiftKey |
                               WebInputEvent::kControlKey |
                               WebInputEvent::kAltKey | WebInputEvent::kMetaKey;

  if (IsAppearanceBaseSelect()) {
    auto* key_event = DynamicTo<KeyboardEvent>(event);
    if (!key_event) {
      // In appearance:base-select mode, all mouse behavior is handled by
      // HTMLFormControlElement's popovertarget implementation. The mouse
      // handling later in this method is for appearance:auto mode only.
      return false;
    }

    bool target_is_button = false;
    if (auto* button = SlottedButton()) {
      for (unsigned i = 0; i < event.GetEventPath().size(); i++) {
        Node& node = event.GetEventPath()[i].GetNode();
        if (node == select_) {
          break;
        } else if (node == button) {
          target_is_button = true;
          break;
        }
      }
    } else if (event.target() == default_button_) {
      target_is_button = true;
    }
    if (!target_is_button) {
      return false;
    }

    if (key_event->GetModifiers() & ignore_modifiers) {
      return false;
    }

    if (event.type() == event_type_names::kKeypress &&
        key_event->key() == "Enter") {
      // Pressing enter on the button should submit the form, not open the
      // popover. HTMLElement::HandleKeypressEvent will fire DOMActivate which
      // opens the popover unless we prevent the default by returning true here.
      return true;
    }

    if (event.type() == event_type_names::kKeydown) {
      if (key_event->key() == "ArrowUp" || key_event->key() == "ArrowDown" ||
          key_event->key() == "ArrowRight" || key_event->key() == "ArrowLeft") {
        // Spacebar already opens the datalist because of the popovertarget
        // association.
        DisplayedDatalist()->ShowPopoverForSelectElement();
        return true;
      } else if (key_event->key() == "Enter") {
        if (auto* form = select_->Form()) {
          form->PrepareForSubmission(&event, select_);
          return true;
        }
      }
    }

    return false;
  }

  const auto* key_event = DynamicTo<KeyboardEvent>(event);
  if (event.type() == event_type_names::kKeydown) {
    if (!select_->GetLayoutObject() || !key_event)
      return false;

    if (ShouldOpenPopupForKeyDownEvent(*key_event))
      return HandlePopupOpenKeyboardEvent();

    // When using spatial navigation, we want to be able to navigate away
    // from the select element when the user hits any of the arrow keys,
    // instead of changing the selection.
    if (IsSpatialNavigationEnabled(select_->GetDocument().GetFrame())) {
      if (!snav_arrow_key_selection_)
        return false;
    }

    // The key handling below shouldn't be used for non spatial navigation
    // mode Mac
    if (LayoutTheme::GetTheme().PopsMenuByArrowKeys() &&
        !IsSpatialNavigationEnabled(select_->GetDocument().GetFrame()))
      return false;

    if (key_event->GetModifiers() & ignore_modifiers)
      return false;

    const String& key = key_event->key();
    bool handled = true;
    HTMLOptionElement* option = select_->SelectedOption();
    int list_index = option ? option->ListIndex() : -1;

    if (key == "ArrowDown" || key == "ArrowRight") {
      option = NextValidOption(list_index, kSkipForwards, 1);
    } else if (key == "ArrowUp" || key == "ArrowLeft") {
      option = NextValidOption(list_index, kSkipBackwards, 1);
    } else if (key == "PageDown") {
      option = NextValidOption(list_index, kSkipForwards, 3);
    } else if (key == "PageUp") {
      option = NextValidOption(list_index, kSkipBackwards, 3);
    } else if (key == "Home") {
      option = FirstSelectableOption();
    } else if (key == "End") {
      option = LastSelectableOption();
    } else {
      handled = false;
    }

    if (handled && option) {
      select_->SelectOption(
          option, HTMLSelectElement::kDeselectOtherOptionsFlag |
                      HTMLSelectElement::kMakeOptionDirtyFlag |
                      HTMLSelectElement::kDispatchInputAndChangeEventFlag);
    }
    return handled;
  }

  if (event.type() == event_type_names::kKeypress) {
    if (!select_->GetLayoutObject() || !key_event)
      return false;

    int key_code = key_event->keyCode();
    if (key_code == ' ' &&
        IsSpatialNavigationEnabled(select_->GetDocument().GetFrame())) {
      // Use space to toggle arrow key handling for selection change or
      // spatial navigation.
      snav_arrow_key_selection_ = !snav_arrow_key_selection_;
      return true;
    }

    if (ShouldOpenPopupForKeyPressEvent(*key_event))
      return HandlePopupOpenKeyboardEvent();

    if (!LayoutTheme::GetTheme().PopsMenuByReturnKey() && key_code == '\r') {
      if (HTMLFormElement* form = select_->Form())
        form->SubmitImplicitly(event, false);
      DispatchEventsIfSelectedOptionChanged();
      return true;
    }
    return false;
  }

  const auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (event.type() == event_type_names::kMousedown && mouse_event &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    InputDeviceCapabilities* source_capabilities =
        select_->GetDocument()
            .domWindow()
            ->GetInputDeviceCapabilities()
            ->FiresTouchEvents(mouse_event->FromTouch());
    select_->Focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                               mojom::blink::FocusType::kMouse,
                               source_capabilities, FocusOptions::Create(),
                               FocusTrigger::kUserGesture));
    if (select_->GetLayoutObject() && !will_be_destroyed_ &&
        !select_->IsDisabledFormControl()) {
      if (native_popup_is_visible_) {
        HidePopup();
      } else {
        // Save the selection so it can be compared to the new selection
        // when we call onChange during selectOption, which gets called
        // from selectOptionByPopup, which gets called after the user
        // makes a selection from the menu.
        SaveLastSelection();
        // TODO(lanwei): Will check if we need to add
        // InputDeviceCapabilities here when select menu list gets
        // focus, see https://crbug.com/476530.
        ShowPopup(mouse_event->FromTouch() ? PopupMenu::kTouch
                                           : PopupMenu::kOther);
      }
    }
    return true;
  }
  return false;
}

bool MenuListSelectType::ShouldOpenPopupForKeyDownEvent(
    const KeyboardEvent& event) {
  const String& key = event.key();
  LayoutTheme& layout_theme = LayoutTheme::GetTheme();

  if (IsSpatialNavigationEnabled(select_->GetDocument().GetFrame()))
    return false;

  return ((layout_theme.PopsMenuByArrowKeys() &&
           (key == "ArrowDown" || key == "ArrowUp")) ||
          ((key == "ArrowDown" || key == "ArrowUp") && event.altKey()) ||
          ((!event.altKey() && !event.ctrlKey() && key == "F4")));
}

bool MenuListSelectType::ShouldOpenPopupForKeyPressEvent(
    const KeyboardEvent& event) {
  LayoutTheme& layout_theme = LayoutTheme::GetTheme();
  int key_code = event.keyCode();

  return ((key_code == ' ' && !select_->type_ahead_.HasActiveSession(event)) ||
          (layout_theme.PopsMenuByReturnKey() && key_code == '\r'));
}

bool MenuListSelectType::HandlePopupOpenKeyboardEvent() {
  select_->Focus(FocusParams(FocusTrigger::kUserGesture));
  // Calling focus() may cause us to lose our LayoutObject. Return true so
  // that our caller doesn't process the event further, but don't set
  // the event as handled.
  if (!select_->GetLayoutObject() || will_be_destroyed_ ||
      select_->IsDisabledFormControl())
    return false;
  // Save the selection so it can be compared to the new selection when
  // dispatching change events during SelectOption, which gets called from
  // SelectOptionByPopup, which gets called after the user makes a selection
  // from the menu.
  SaveLastSelection();
  ShowPopup(PopupMenu::kOther);
  return true;
}

void MenuListSelectType::CreateShadowSubtree(ShadowRoot& root) {
  Document& doc = select_->GetDocument();

  inner_element_ = MakeGarbageCollected<MenuListInnerElement>(doc);
  inner_element_->setAttribute(html_names::kAriaHiddenAttr, keywords::kTrue);
  // Make sure InnerElementForAppearanceAuto() always has a Text node.
  inner_element_->appendChild(Text::Create(doc, g_empty_string));
  root.AppendChild(inner_element_);

  // Even in MenuList mode, slotting <option>s is necessary to have
  // ComputedStyles for <option>s. LayoutFlexibleBox::IsChildAllowed() rejects
  // all of LayoutObject children except for MenuListInnerElement's.
  // This slot does not have anything slotted into it in the StylableSelect mode
  // because the <datalist> containing all the <option>s is slotted in instead.
  option_slot_ = MakeGarbageCollected<HTMLSlotElement>(doc);
  option_slot_->SetIdAttribute(shadow_element_names::kSelectOptions);
  root.appendChild(option_slot_);

  if (RuntimeEnabledFeatures::StylableSelectEnabled()) {
    button_slot_ = MakeGarbageCollected<HTMLSlotElement>(doc);
    button_slot_->SetIdAttribute(shadow_element_names::kSelectButton);
    root.appendChild(button_slot_);

    default_button_ = MakeGarbageCollected<HTMLButtonElement>(doc);
    default_button_->setAttribute(html_names::kTypeAttr,
                                  AtomicString("popover"));
    button_slot_->AppendChild(default_button_);

    datalist_slot_ = MakeGarbageCollected<HTMLSlotElement>(doc);
    datalist_slot_->SetIdAttribute(shadow_element_names::kSelectDatalist);
    root.appendChild(datalist_slot_);

    default_datalist_ = MakeGarbageCollected<HTMLDataListElement>(doc);
    default_datalist_->SetShadowPseudoId(
        shadow_element_names::kSelectFallbackDatalist);
    datalist_slot_->AppendChild(default_datalist_);
    default_datalist_options_slot_ = MakeGarbageCollected<HTMLSlotElement>(doc);
    default_datalist_->AppendChild(default_datalist_options_slot_);
  }
}

void MenuListSelectType::ManuallyAssignSlots() {
  VectorOf<Node> option_nodes;
  VectorOf<Node> buttons;
  Node* first_datalist = nullptr;
  for (Node& child : NodeTraversal::ChildrenOf(*select_)) {
    if (!child.IsSlotable()) {
      continue;
    }
    if (CanAssignToSelectSlot(child)) {
      option_nodes.push_back(child);
    } else if (IsA<HTMLButtonElement>(child)) {
      buttons.push_back(child);
    } else if (!first_datalist && IsA<HTMLDataListElement>(child)) {
      first_datalist = &child;
    }
  }

  if (RuntimeEnabledFeatures::StylableSelectEnabled()) {
    button_slot_->Assign(buttons);
    datalist_slot_->Assign(first_datalist);
    if (default_datalist_->popoverOpen()) {
      default_datalist_options_slot_->Assign(option_nodes);
    } else {
      option_slot_->Assign(option_nodes);
    }
  } else {
    option_slot_->Assign(option_nodes);
  }
}

HTMLButtonElement* MenuListSelectType::SlottedButton() const {
  if (!RuntimeEnabledFeatures::StylableSelectEnabled()) {
    CHECK(!button_slot_);
    return nullptr;
  }
  CHECK(button_slot_);
  return To<HTMLButtonElement>(button_slot_->FirstAssignedNode());
}

HTMLDataListElement* MenuListSelectType::DisplayedDatalist() const {
  if (!RuntimeEnabledFeatures::StylableSelectEnabled()) {
    CHECK(!datalist_slot_);
    return nullptr;
  }
  return To<HTMLDataListElement>(
      FlatTreeTraversal::FirstChild(*datalist_slot_));
}

bool MenuListSelectType::IsAppearanceBaseSelect() const {
  if (!RuntimeEnabledFeatures::StylableSelectEnabled()) {
    return false;
  }
  if (auto* style = select_->GetComputedStyle()) {
    return style->EffectiveAppearance() == ControlPart::kBaseSelectPart;
  }
  return false;
}

Element& MenuListSelectType::InnerElementForAppearanceAuto() const {
  return *inner_element_;
}

void MenuListSelectType::ShowPopup(PopupMenu::ShowEventType type) {
  if (PopupIsVisible()) {
    return;
  }

  if (IsAppearanceBaseSelect()) {
    select_->DisplayedDatalist()->ShowPopoverForSelectElement();
    return;
  }

  Document& document = select_->GetDocument();
  if (document.GetPage()->GetChromeClient().HasOpenedPopup())
    return;
  if (!select_->GetLayoutObject())
    return;

  gfx::Rect local_root_rect = select_->VisibleBoundsInLocalRoot();

  if (document.GetFrame()->LocalFrameRoot().IsOutermostMainFrame()) {
    gfx::Rect visual_viewport_rect =
        document.GetPage()->GetVisualViewport().RootFrameToViewport(
            local_root_rect);
    visual_viewport_rect.Intersect(
        gfx::Rect(document.GetPage()->GetVisualViewport().Size()));
    if (visual_viewport_rect.IsEmpty())
      return;
  } else {
    // TODO(bokan): If we're in a remote frame, we cannot access the active
    // visual viewport. VisibleBoundsInLocalRoot will clip to the outermost
    // main frame but if the user is pinch-zoomed this won't be accurate.
    // https://crbug.com/840944.
    if (local_root_rect.IsEmpty())
      return;
  }

  if (!popup_) {
    popup_ = document.GetPage()->GetChromeClient().OpenPopupMenu(
        *document.GetFrame(), *select_);
  }
  if (!popup_)
    return;

  SetNativePopupIsVisible(true);
  ObserveTreeMutation();

  popup_->Show(type);
  if (AXObjectCache* cache = document.ExistingAXObjectCache())
    cache->DidShowMenuListPopup(select_->GetLayoutObject());
}

void MenuListSelectType::HidePopup() {
  if (IsAppearanceBaseSelect()) {
    DisplayedDatalist()->HidePopoverForSelectElement();
    return;
  }
  if (popup_)
    popup_->Hide();
}

void MenuListSelectType::PopupDidHide() {
  SetNativePopupIsVisible(false);
  UnobserveTreeMutation();
  if (AXObjectCache* cache = select_->GetDocument().ExistingAXObjectCache()) {
    if (auto* layout_object = select_->GetLayoutObject())
      cache->DidHideMenuListPopup(layout_object);
  }
}

bool MenuListSelectType::PopupIsVisible() const {
  if (IsAppearanceBaseSelect()) {
    return DisplayedDatalist()->popoverOpen();
  } else {
    return native_popup_is_visible_;
  }
}

void MenuListSelectType::SetNativePopupIsVisible(bool popup_is_visible) {
  native_popup_is_visible_ = popup_is_visible;
  if (auto* layout_object = select_->GetLayoutObject()) {
    // Invalidate paint to ensure that the focus ring is updated.
    layout_object->SetShouldDoFullPaintInvalidation();
  }
}

PopupMenu* MenuListSelectType::PopupForTesting() const {
  return popup_.Get();
}

AXObject* MenuListSelectType::PopupRootAXObject() const {
  return popup_ ? popup_->PopupRootAXObject() : nullptr;
}

void MenuListSelectType::ShowPicker() {
  // We need to make the layout tree up-to-date to have GetLayoutObject() give
  // the correct result below. An author event handler may have set display to
  // some element to none which will cause a layout tree detach.
  select_->GetDocument().UpdateStyleAndLayoutTree();
  // Save the selection so it can be compared to the new selection
  // when we call onChange during selectOption, which gets called
  // from selectOptionByPopup, which gets called after the user
  // makes a selection from the menu.
  SaveLastSelection();
  ShowPopup(PopupMenu::kOther);
}

void MenuListSelectType::DidSelectOption(
    HTMLOptionElement* element,
    HTMLSelectElement::SelectOptionFlags flags,
    bool should_update_popup) {
  // Need to update last_on_change_option_ before UpdateFromElement().
  const bool should_dispatch_events =
      (flags & HTMLSelectElement::kDispatchInputAndChangeEventFlag) &&
      select_->last_on_change_option_ != element;
  select_->last_on_change_option_ = element;

  UpdateTextStyleAndContent();
  // PopupMenu::UpdateFromElement() posts an O(N) task.
  if (native_popup_is_visible_ && should_update_popup) {
    popup_->UpdateFromElement(PopupMenu::kBySelectionChange);
  }

  select_->SetNeedsValidityCheck();

  if (should_dispatch_events) {
    select_->DispatchInputEvent();
    select_->DispatchChangeEvent();
  }
  if (select_->GetLayoutObject()) {
    // Need to check will_be_destroyed_ because event handlers might
    // disassociate |this| and select_.
    if (!will_be_destroyed_) {
      // DidUpdateActiveOption() is O(N) because of HTMLOptionElement::index().
      DidUpdateActiveOption(element);
    }
  }
}

void MenuListSelectType::DispatchEventsIfSelectedOptionChanged() {
  HTMLOptionElement* selected_option = select_->SelectedOption();
  if (select_->last_on_change_option_.Get() != selected_option) {
    select_->last_on_change_option_ = selected_option;
    select_->DispatchInputEvent();
    select_->DispatchChangeEvent();
  }
}

void MenuListSelectType::DidBlur() {
  // We only need to fire change events here for menu lists, because we fire
  // change events for list boxes whenever the selection change is actually
  // made.  This matches other browsers' behavior.
  DispatchEventsIfSelectedOptionChanged();
  if (PopupIsVisible())
    HidePopup();
}

void MenuListSelectType::DidSetSuggestedOption(HTMLOptionElement*) {
  UpdateTextStyleAndContent();
  if (native_popup_is_visible_) {
    popup_->UpdateFromElement(PopupMenu::kBySelectionChange);
  }
}

void MenuListSelectType::SaveLastSelection() {
  select_->last_on_change_option_ = select_->SelectedOption();
}

void MenuListSelectType::DidDetachLayoutTree() {
  if (popup_)
    popup_->DisconnectClient();
  SetNativePopupIsVisible(false);
  popup_ = nullptr;
  UnobserveTreeMutation();
}

void MenuListSelectType::DidRecalcStyle(const StyleRecalcChange change) {
  if (auto* style = select_->GetComputedStyle()) {
    bool is_appearance_base_select =
        style->EffectiveAppearance() == ControlPart::kBaseSelectPart;
    if (is_appearance_base_select_ != is_appearance_base_select) {
      is_appearance_base_select_ = is_appearance_base_select;
      // Switching appearance needs layout to be rebuilt because of special
      // logic in LayoutFlexibleBox::IsChildAllowed which ignores children in
      // appearance:auto mode. We also call SetNeedsReattachLayoutTree every
      // time that the size and multiple attributes are changed.
      select_->SetNeedsReattachLayoutTree();

      // In appearance:base-select mode, we want the child button to get focus
      // instead of the <select> itself.
      select_->GetShadowRoot()->SetDelegatesFocus(is_appearance_base_select);
    }
  }

  if (change.ReattachLayoutTree())
    return;
  UpdateTextStyle();
  if (auto* layout_object = select_->GetLayoutObject()) {
    // Invalidate paint to ensure that the focus ring is updated.
    layout_object->SetShouldDoFullPaintInvalidation();
  }
  if (native_popup_is_visible_) {
    popup_->UpdateFromElement(PopupMenu::kByStyleChange);
  }
}

String MenuListSelectType::UpdateTextStyleInternal() {
  HTMLOptionElement* option_to_be_shown = OptionToBeShown();
  String text = g_empty_string;
  const ComputedStyle* option_style = nullptr;

  if (select_->IsMultiple()) {
    unsigned selected_count = 0;
    HTMLOptionElement* selected_option_element = nullptr;
    for (auto* const option : select_->GetOptionList()) {
      if (option->Selected()) {
        if (++selected_count == 1)
          selected_option_element = option;
      }
    }

    if (selected_count == 1) {
      text = selected_option_element->TextIndentedToRespectGroupLabel();
      option_style = selected_option_element->GetComputedStyle();
    } else {
      Locale& locale = select_->GetLocale();
      String localized_number_string =
          locale.ConvertToLocalizedNumber(String::Number(selected_count));
      text = locale.QueryString(IDS_FORM_SELECT_MENU_LIST_TEXT,
                                localized_number_string);
      DCHECK(!option_style);
    }
  } else {
    if (option_to_be_shown) {
      text = option_to_be_shown->TextIndentedToRespectGroupLabel();
      option_style = option_to_be_shown->GetComputedStyle();
    }
  }
  option_style_ = option_style;

  auto& inner_element = select_->InnerElementForAppearanceAuto();
  const ComputedStyle* inner_style = inner_element.GetComputedStyle();
  if (inner_style && option_style &&
      ((option_style->Direction() != inner_style->Direction() ||
        option_style->GetUnicodeBidi() != inner_style->GetUnicodeBidi() ||
        option_style->GetTextAlign(true) != inner_style->GetTextAlign(true)))) {
    ComputedStyleBuilder builder(*inner_style);
    builder.SetDirection(option_style->Direction());
    builder.SetUnicodeBidi(option_style->GetUnicodeBidi());
    builder.SetTextAlign(option_style->GetTextAlign(true));
    const ComputedStyle* new_style = builder.TakeStyle();
    if (auto* inner_layout = inner_element.GetLayoutObject()) {
      inner_layout->SetModifiedStyleOutsideStyleRecalc(
          new_style, LayoutObject::ApplyStyleChanges::kYes);
    } else {
      inner_element.SetComputedStyle(std::move(new_style));
    }
  }
  if (select_->GetLayoutObject())
    DidUpdateActiveOption(option_to_be_shown);

  return text.StripWhiteSpace();
}

void MenuListSelectType::UpdateTextStyleAndContent() {
  String text = UpdateTextStyleInternal();
  select_->InnerElementForAppearanceAuto().firstChild()->setNodeValue(text);
  if (RuntimeEnabledFeatures::StylableSelectEnabled()) {
    // Copy the text of the selected <option> into the fallback <button> so that
    // the user can see what the selected option is, just like the
    // appearance:auto case.
    default_button_->setTextContent(text);
  }
  if (auto* box = select_->GetLayoutBox()) {
    if (auto* cache = select_->GetDocument().ExistingAXObjectCache())
      cache->TextChanged(box);
  }
}

void MenuListSelectType::DidUpdateActiveOption(HTMLOptionElement* option) {
  Document& document = select_->GetDocument();
  if (!document.ExistingAXObjectCache())
    return;

  int option_index = option ? option->index() : -1;
  if (ax_menulist_last_active_index_ == option_index)
    return;
  ax_menulist_last_active_index_ = option_index;

  // We skip sending accessiblity notifications for the very first option,
  // otherwise we get extra focus and select events that are undesired.
  if (!has_updated_menulist_active_option_) {
    has_updated_menulist_active_option_ = true;
    return;
  }

  document.ExistingAXObjectCache()->HandleUpdateActiveMenuOption(select_);
}

HTMLOptionElement* MenuListSelectType::OptionToBeShown() const {
  if (auto* option =
          select_->OptionAtListIndex(select_->index_to_select_on_cancel_))
    return option;
  if (select_->suggested_option_)
    return select_->suggested_option_.Get();
  // TODO(tkent): We should not call OptionToBeShown() in IsMultiple() case.
  if (select_->IsMultiple())
    return select_->SelectedOption();
  DCHECK_EQ(select_->SelectedOption(), select_->last_on_change_option_);
  return select_->last_on_change_option_.Get();
}

void MenuListSelectType::MaximumOptionWidthMightBeChanged() const {
  if (LayoutObject* layout_object = select_->GetLayoutObject()) {
    layout_object->SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kMenuOptionsChanged);
  }
}

// PopupUpdater notifies updates of the specified SELECT element subtree to
// a PopupMenu object.
class PopupUpdater : public MutationObserver::Delegate {
 public:
  explicit PopupUpdater(MenuListSelectType& select_type,
                        HTMLSelectElement& select)
      : select_type_(select_type),
        select_(select),
        observer_(MutationObserver::Create(this)) {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setAttributeOldValue(true);
    init->setAttributes(true);
    // Observe only attributes which affect popup content.
    init->setAttributeFilter({"disabled", "label", "selected", "value"});
    init->setCharacterData(true);
    init->setCharacterDataOldValue(true);
    init->setChildList(true);
    init->setSubtree(true);
    observer_->observe(select_, init, ASSERT_NO_EXCEPTION);
  }

  ExecutionContext* GetExecutionContext() const override {
    return select_->GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    // We disconnect the MutationObserver when a popup is closed.  However
    // MutationObserver can call back after disconnection.
    if (!select_type_->PopupIsVisible())
      return;
    for (const auto& record : records) {
      if (record->type() == "attributes") {
        const auto& element = *To<Element>(record->target());
        if (record->oldValue() == element.getAttribute(record->attributeName()))
          continue;
      } else if (record->type() == "characterData") {
        if (record->oldValue() == record->target()->nodeValue())
          continue;
      }
      select_type_->DidMutateSubtree();
      return;
    }
  }

  void Dispose() { observer_->disconnect(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(select_type_);
    visitor->Trace(select_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<MenuListSelectType> select_type_;
  Member<HTMLSelectElement> select_;
  Member<MutationObserver> observer_;
};

void MenuListSelectType::ObserveTreeMutation() {
  DCHECK(!popup_updater_);
  popup_updater_ = MakeGarbageCollected<PopupUpdater>(*this, *select_);
}

void MenuListSelectType::UnobserveTreeMutation() {
  if (!popup_updater_)
    return;
  popup_updater_->Dispose();
  popup_updater_ = nullptr;
}

void MenuListSelectType::DidMutateSubtree() {
  DCHECK(native_popup_is_visible_);
  DCHECK(popup_);
  popup_->UpdateFromElement(PopupMenu::kByDOMChange);
}

// ============================================================================

// TODO(crbug.com/1511354): Rename this class to InPageSelectType
class ListBoxSelectType final : public SelectType {
 public:
  explicit ListBoxSelectType(HTMLSelectElement& select) : SelectType(select) {}
  void Trace(Visitor* visitor) const override;

  bool DefaultEventHandler(const Event& event) override;
  void DidSelectOption(HTMLOptionElement* element,
                       HTMLSelectElement::SelectOptionFlags flags,
                       bool should_update_popup) override;
  void OptionRemoved(HTMLOptionElement& option) override;
  void DidBlur() override;
  void DidSetSuggestedOption(HTMLOptionElement* option) override;
  void SaveLastSelection() override;
  HTMLOptionElement* SpatialNavigationFocusedOption() override;
  HTMLOptionElement* ActiveSelectionEnd() const override;
  void ScrollToSelection() override;
  void ScrollToOption(HTMLOptionElement* option) override;
  void SelectAll() override;
  void SaveListboxActiveSelection() override;
  void HandleMouseRelease() override;
  void ListBoxOnChange() override;
  void ClearLastOnChangeSelection() override;
  void CreateShadowSubtree(ShadowRoot&) override;
  void ManuallyAssignSlots() override;
  HTMLButtonElement* SlottedButton() const override;
  HTMLDataListElement* DisplayedDatalist() const override;
  bool IsAppearanceBaseSelect() const override;

 private:
  HTMLOptionElement* NextSelectableOptionPageAway(HTMLOptionElement*,
                                                  SkipDirection) const;
  // Update :-internal-multi-select-focus state of selected OPTIONs.
  void UpdateMultiSelectFocus();
  void ToggleSelection(HTMLOptionElement& option);
  enum class SelectionMode {
    kDeselectOthers,
    kRange,
    kNotChangeOthers,
  };
  void UpdateSelectedState(HTMLOptionElement* clicked_option,
                           SelectionMode mode);
  void UpdateListBoxSelection(bool deselect_other_options, bool scroll = true);
  void SetActiveSelectionAnchor(HTMLOptionElement*);
  void SetActiveSelectionEnd(HTMLOptionElement*);
  void ScrollToOptionTask();

  Vector<bool> cached_state_for_active_selection_;
  Vector<bool> last_on_change_selection_;
  Member<HTMLOptionElement> option_to_scroll_to_;
  Member<HTMLOptionElement> active_selection_anchor_;
  Member<HTMLOptionElement> active_selection_end_;
  // TODO(crbug.com/1511354): Remove option_slot_ when the StylableSelect flag
  // is enabled and removed. It is only used when StylableSelect is disabled.
  Member<HTMLSlotElement> option_slot_;
  bool is_in_non_contiguous_selection_ = false;
  bool active_selection_state_ = false;
};

void ListBoxSelectType::Trace(Visitor* visitor) const {
  visitor->Trace(option_to_scroll_to_);
  visitor->Trace(active_selection_anchor_);
  visitor->Trace(active_selection_end_);
  visitor->Trace(option_slot_);
  SelectType::Trace(visitor);
}

bool ListBoxSelectType::DefaultEventHandler(const Event& event) {
  const auto* mouse_event = DynamicTo<MouseEvent>(event);
  const auto* gesture_event = DynamicTo<GestureEvent>(event);
  if (event.type() == event_type_names::kGesturetap && gesture_event) {
    select_->Focus(FocusParams(FocusTrigger::kUserGesture));
    // Calling focus() may cause us to lose our layoutObject or change the
    // layoutObject type, in which case do not want to handle the event.
    if (!select_->GetLayoutObject() || will_be_destroyed_)
      return false;

    // Convert to coords relative to the list box if needed.
    if (HTMLOptionElement* option = EventTargetOption(*gesture_event)) {
      if (!select_->IsDisabledFormControl()) {
        UpdateSelectedState(option, gesture_event->shiftKey()
                                        ? SelectionMode::kRange
                                        : SelectionMode::kNotChangeOthers);
        ListBoxOnChange();
      }
      return true;
    }
    return false;
  }

  if (event.type() == event_type_names::kMousedown && mouse_event &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    select_->Focus(FocusParams(FocusTrigger::kUserGesture));
    // Calling focus() may cause us to lose our layoutObject, in which case
    // do not want to handle the event.
    if (!select_->GetLayoutObject() || will_be_destroyed_ ||
        select_->IsDisabledFormControl())
      return false;

    // Convert to coords relative to the list box if needed.
    if (HTMLOptionElement* option = EventTargetOption(*mouse_event)) {
      if (!option->IsDisabledFormControl()) {
#if BUILDFLAG(IS_MAC)
        const bool meta_or_ctrl = mouse_event->metaKey();
#else
        const bool meta_or_ctrl = mouse_event->ctrlKey();
#endif
        UpdateSelectedState(option, mouse_event->shiftKey()
                                        ? SelectionMode::kRange
                                        : meta_or_ctrl
                                              ? SelectionMode::kNotChangeOthers
                                              : SelectionMode::kDeselectOthers);
      }
      if (LocalFrame* frame = select_->GetDocument().GetFrame())
        frame->GetEventHandler().SetMouseDownMayStartAutoscroll();

      return true;
    }
    return false;
  }

  if (event.type() == event_type_names::kMousemove && mouse_event) {
    if (mouse_event->button() !=
            static_cast<int16_t>(WebPointerProperties::Button::kLeft) ||
        !mouse_event->ButtonDown())
      return false;

    if (auto* layout_object = select_->GetLayoutObject()) {
      layout_object->GetFrameView()->UpdateAllLifecyclePhasesExceptPaint(
          DocumentUpdateReason::kScroll);
    }
    // Lifecycle update could have detached the layout object.
    if (auto* layout_object = select_->GetLayoutObject()) {
      if (Page* page = select_->GetDocument().GetPage()) {
        page->GetAutoscrollController().StartAutoscrollForSelection(
            layout_object);
      }
    }
    // Mousedown didn't happen in this element.
    if (last_on_change_selection_.empty())
      return false;

    if (HTMLOptionElement* option = EventTargetOption(*mouse_event)) {
      if (!select_->IsDisabledFormControl()) {
        if (select_->is_multiple_) {
          // Only extend selection if there is something selected.
          if (!active_selection_anchor_)
            return false;

          SetActiveSelectionEnd(option);
          UpdateListBoxSelection(false);
        } else {
          SetActiveSelectionAnchor(option);
          SetActiveSelectionEnd(option);
          UpdateListBoxSelection(true);
        }
      }
    }
    return false;
  }

  if (event.type() == event_type_names::kMouseup && mouse_event &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft) &&
      select_->GetLayoutObject()) {
    auto* page = select_->GetDocument().GetPage();
    if (page && page->GetAutoscrollController().AutoscrollInProgressFor(
                    select_->GetLayoutBox()))
      page->GetAutoscrollController().StopAutoscroll();
    else
      HandleMouseRelease();
    return false;
  }

  if (event.type() == event_type_names::kKeydown) {
    const auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (!keyboard_event)
      return false;
    const String& key = keyboard_event->key();

    bool handled = false;
    HTMLOptionElement* end_option = nullptr;
    char const* key_next = "ArrowDown";
    char const* key_previous = "ArrowUp";
    const ComputedStyle* style = select_->GetComputedStyle();
    if (style->GetWritingMode() == WritingMode::kVerticalLr) {
      key_next = "ArrowRight";
      key_previous = "ArrowLeft";
    } else if (style->GetWritingMode() == WritingMode::kVerticalRl) {
      key_next = "ArrowLeft";
      key_previous = "ArrowRight";
    }
    if (!active_selection_end_) {
      // Initialize the end index
      if (key == key_next || key == "PageDown") {
        HTMLOptionElement* start_option = select_->LastSelectedOption();
        handled = true;
        if (key == key_next) {
          end_option = NextSelectableOption(start_option);
        } else {
          end_option =
              NextSelectableOptionPageAway(start_option, kSkipForwards);
        }
      } else if (key == key_previous || key == "PageUp") {
        HTMLOptionElement* start_option = select_->SelectedOption();
        handled = true;
        if (key == key_previous) {
          end_option = PreviousSelectableOption(start_option);
        } else {
          end_option =
              NextSelectableOptionPageAway(start_option, kSkipBackwards);
        }
      }
    } else {
      // Set the end index based on the current end index.
      if (key == key_next) {
        end_option = NextSelectableOption(active_selection_end_);
        handled = true;
      } else if (key == key_previous) {
        end_option = PreviousSelectableOption(active_selection_end_);
        handled = true;
      } else if (key == "PageDown") {
        end_option =
            NextSelectableOptionPageAway(active_selection_end_, kSkipForwards);
        handled = true;
      } else if (key == "PageUp") {
        end_option =
            NextSelectableOptionPageAway(active_selection_end_, kSkipBackwards);
        handled = true;
      }
    }
    if (key == "Home") {
      end_option = FirstSelectableOption();
      handled = true;
    } else if (key == "End") {
      end_option = LastSelectableOption();
      handled = true;
    }

    if (IsSpatialNavigationEnabled(select_->GetDocument().GetFrame())) {
      // Check if the selection moves to the boundary.
      if (key == "ArrowLeft" || key == "ArrowRight" ||
          ((key == "ArrowDown" || key == "ArrowUp") &&
           end_option == active_selection_end_))
        return false;
    }

    bool is_control_key = false;
#if BUILDFLAG(IS_MAC)
    is_control_key = keyboard_event->metaKey();
#else
    is_control_key = keyboard_event->ctrlKey();
#endif

    if (select_->is_multiple_ && keyboard_event->keyCode() == ' ' &&
        is_control_key && active_selection_end_) {
      // Use ctrl+space to toggle selection change.
      ToggleSelection(*active_selection_end_);
      return true;
    }

    if (end_option && handled) {
      // Save the selection so it can be compared to the new selection
      // when dispatching change events immediately after making the new
      // selection.
      SaveLastSelection();

      SetActiveSelectionEnd(end_option);

      is_in_non_contiguous_selection_ = select_->is_multiple_ && is_control_key;
      bool select_new_item =
          !select_->is_multiple_ || keyboard_event->shiftKey() ||
          (!IsSpatialNavigationEnabled(select_->GetDocument().GetFrame()) &&
           !is_in_non_contiguous_selection_);
      if (select_new_item)
        active_selection_state_ = true;
      // If the anchor is uninitialized, or if we're going to deselect all
      // other options, then set the anchor index equal to the end index.
      bool deselect_others = !select_->is_multiple_ ||
                             (!keyboard_event->shiftKey() && select_new_item);
      if (!active_selection_anchor_ || deselect_others) {
        if (deselect_others)
          select_->DeselectItemsWithoutValidation();
        SetActiveSelectionAnchor(active_selection_end_.Get());
      }

      ScrollToOption(end_option);
      if (select_new_item || is_in_non_contiguous_selection_) {
        if (select_new_item) {
          UpdateListBoxSelection(deselect_others);
          ListBoxOnChange();
        }
        UpdateMultiSelectFocus();
      } else {
        ScrollToSelection();
      }

      return true;
    }
    return false;
  }

  if (event.type() == event_type_names::kKeypress) {
    auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (!keyboard_event)
      return false;
    int key_code = keyboard_event->keyCode();

    if (key_code == '\r') {
      if (HTMLFormElement* form = select_->Form())
        form->SubmitImplicitly(event, false);
      return true;
    } else if (select_->is_multiple_ && key_code == ' ' &&
               (IsSpatialNavigationEnabled(select_->GetDocument().GetFrame()) ||
                is_in_non_contiguous_selection_)) {
      HTMLOptionElement* option = active_selection_end_;
      // If there's no active selection,
      // act as if "ArrowDown" had been pressed.
      if (!option)
        option = NextSelectableOption(select_->LastSelectedOption());
      if (option) {
        // Use space to toggle selection change.
        ToggleSelection(*option);
        return true;
      }
    }
    return false;
  }
  return false;
}

void ListBoxSelectType::DidSelectOption(
    HTMLOptionElement* element,
    HTMLSelectElement::SelectOptionFlags flags,
    bool should_update_popup) {
  // We should update active selection after finishing OPTION state change
  // because SetActiveSelectionAnchor() stores OPTION's selection state.
  if (element) {
    const bool is_single = !select_->IsMultiple();
    const bool deselect_other_options =
        flags & HTMLSelectElement::kDeselectOtherOptionsFlag;
    // SetActiveSelectionAnchor is O(N).
    if (!active_selection_anchor_ || is_single || deselect_other_options)
      SetActiveSelectionAnchor(element);
    if (!active_selection_end_ || is_single || deselect_other_options)
      SetActiveSelectionEnd(element);
  }

  ScrollToSelection();
  select_->SetNeedsValidityCheck();
}

void ListBoxSelectType::OptionRemoved(HTMLOptionElement& option) {
  if (option_to_scroll_to_ == &option)
    option_to_scroll_to_.Clear();
  if (active_selection_anchor_ == &option)
    active_selection_anchor_.Clear();
  if (active_selection_end_ == &option)
    active_selection_end_.Clear();
}

void ListBoxSelectType::DidBlur() {
  ClearLastOnChangeSelection();
}

void ListBoxSelectType::DidSetSuggestedOption(HTMLOptionElement* option) {
  if (!select_->GetLayoutObject())
    return;
  // When ending preview state, don't leave the scroll position at the
  // previewed element but return to the active selection end if it is
  // defined or to the first selectable option. See crbug.com/1261689.
  if (!option)
    option = ActiveSelectionEnd();
  if (!option)
    option = FirstSelectableOption();
  ScrollToOption(option);
}

void ListBoxSelectType::SaveLastSelection() {
  last_on_change_selection_.clear();
  for (auto& element : select_->GetListItems()) {
    auto* option_element = DynamicTo<HTMLOptionElement>(element.Get());
    last_on_change_selection_.push_back(option_element &&
                                        option_element->Selected());
  }
}

void ListBoxSelectType::UpdateMultiSelectFocus() {
  if (!select_->is_multiple_)
    return;

  for (auto* const option : select_->GetOptionList()) {
    if (option->IsDisabledFormControl() || !option->GetLayoutObject())
      continue;
    bool is_focused =
        (option == active_selection_end_) && is_in_non_contiguous_selection_;
    option->SetMultiSelectFocusedState(is_focused);
  }
  ScrollToSelection();
}

HTMLOptionElement* ListBoxSelectType::SpatialNavigationFocusedOption() {
  if (!IsSpatialNavigationEnabled(select_->GetDocument().GetFrame()))
    return nullptr;
  if (HTMLOptionElement* option = ActiveSelectionEnd())
    return option;
  return FirstSelectableOption();
}

void ListBoxSelectType::SetActiveSelectionAnchor(HTMLOptionElement* option) {
  active_selection_anchor_ = option;
  SaveListboxActiveSelection();
}

void ListBoxSelectType::SetActiveSelectionEnd(HTMLOptionElement* option) {
  active_selection_end_ = option;
}

HTMLOptionElement* ListBoxSelectType::ActiveSelectionEnd() const {
  if (active_selection_end_)
    return active_selection_end_.Get();
  return select_->LastSelectedOption();
}

void ListBoxSelectType::ScrollToSelection() {
  if (!select_->IsFinishedParsingChildren())
    return;
  ScrollToOption(ActiveSelectionEnd());
  if (AXObjectCache* cache = select_->GetDocument().ExistingAXObjectCache())
    cache->ListboxActiveIndexChanged(select_);
}

void ListBoxSelectType::ScrollToOption(HTMLOptionElement* option) {
  if (!option)
    return;
  bool has_pending_task = option_to_scroll_to_ != nullptr;
  // We'd like to keep an HTMLOptionElement reference rather than the index of
  // the option because the task should work even if unselected option is
  // inserted before executing ScrollToOptionTask().
  option_to_scroll_to_ = option;
  if (!has_pending_task) {
    select_->GetDocument()
        .GetTaskRunner(TaskType::kUserInteraction)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&ListBoxSelectType::ScrollToOptionTask,
                                 WrapPersistent(this)));
  }
}

void ListBoxSelectType::ScrollToOptionTask() {
  HTMLOptionElement* option = option_to_scroll_to_.Release();
  if (!option || !select_->isConnected() || will_be_destroyed_)
    return;
  // OptionRemoved() makes sure option_to_scroll_to_ doesn't have an option
  // with another owner.
  DCHECK_EQ(option->OwnerSelectElement(), select_);
  select_->GetDocument().UpdateStyleAndLayoutForNode(
      select_, DocumentUpdateReason::kScroll);
  if (!select_->GetLayoutObject())
    return;
  PhysicalRect bounds = option->BoundingBoxForScrollIntoView();

  // The following code will not scroll parent boxes unlike ScrollRectToVisible.
  auto* box = select_->GetLayoutBox();
  if (!box->IsScrollContainer())
    return;
  DCHECK(box->Layer());
  DCHECK(box->Layer()->GetScrollableArea());
  box->Layer()->GetScrollableArea()->ScrollIntoView(
      bounds, PhysicalBoxStrut(),
      ScrollAlignment::CreateScrollIntoViewParams(
          ScrollAlignment::ToEdgeIfNeeded(), ScrollAlignment::ToEdgeIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic, false,
          mojom::blink::ScrollBehavior::kInstant));
}

void ListBoxSelectType::SelectAll() {
  if (!select_->GetLayoutObject() || !select_->is_multiple_)
    return;

  // Save the selection so it can be compared to the new selectAll selection
  // when dispatching change events.
  SaveLastSelection();

  active_selection_state_ = true;
  SetActiveSelectionAnchor(NextSelectableOption(nullptr));
  SetActiveSelectionEnd(PreviousSelectableOption(nullptr));

  UpdateListBoxSelection(false, false);
  ListBoxOnChange();
  select_->SetNeedsValidityCheck();
}

// Returns the index of the next valid item one page away from |start_option|
// in direction |direction|.
HTMLOptionElement* ListBoxSelectType::NextSelectableOptionPageAway(
    HTMLOptionElement* start_option,
    SkipDirection direction) const {
  const auto& items = select_->GetListItems();
  // -1 so we still show context.
  int page_size = select_->ListBoxSize() - 1;

  // One page away, but not outside valid bounds.
  // If there is a valid option item one page away, the index is chosen.
  // If there is no exact one page away valid option, returns start_index or
  // the most far index.
  int start_index = start_option ? start_option->ListIndex() : -1;
  int edge_index = (direction == kSkipForwards) ? 0 : (items.size() - 1);
  int skip_amount =
      page_size +
      ((direction == kSkipForwards) ? start_index : (edge_index - start_index));
  return NextValidOption(edge_index, direction, skip_amount);
}

void ListBoxSelectType::ToggleSelection(HTMLOptionElement& option) {
  active_selection_state_ = !active_selection_state_;
  UpdateSelectedState(&option, SelectionMode::kNotChangeOthers);
  ListBoxOnChange();
}

void ListBoxSelectType::UpdateSelectedState(HTMLOptionElement* clicked_option,
                                            SelectionMode mode) {
  DCHECK(clicked_option);
  // Save the selection so it can be compared to the new selection when
  // dispatching change events during mouseup, or after autoscroll finishes.
  SaveLastSelection();

  if (!select_->is_multiple_)
    mode = SelectionMode::kDeselectOthers;

  // Keep track of whether an active selection (like during drag selection),
  // should select or deselect.
  active_selection_state_ =
      !(clicked_option->Selected() && mode == SelectionMode::kNotChangeOthers);

  // If we're not in any special multiple selection mode, then deselect all
  // other items, excluding the clicked OPTION. If no option was clicked,
  // then this will deselect all items in the list.
  if (mode == SelectionMode::kDeselectOthers) {
    bool did_deselect_others =
        select_->DeselectItemsWithoutValidation(clicked_option);
    // In a multi-select, if nothing else could be deselected,
    // deselect the (already selected) clicked option instead.
    if (select_->is_multiple_ && !did_deselect_others &&
        clicked_option->Selected() &&
        RuntimeEnabledFeatures::MultiSelectDeselectWhenOnlyOptionEnabled()) {
      active_selection_state_ = false;
    }
  }

  // If the anchor hasn't been set, and we're doing kDeselectOthers or kRange,
  // then initialize the anchor to the first selected OPTION.
  if (!active_selection_anchor_ && mode != SelectionMode::kNotChangeOthers)
    SetActiveSelectionAnchor(select_->SelectedOption());

  // Set the selection state of the clicked OPTION.
  if (!clicked_option->IsDisabledFormControl()) {
    clicked_option->SetSelectedState(active_selection_state_);
    clicked_option->SetDirty(true);
  }

  // If there was no selectedIndex() for the previous initialization, or if
  // we're doing kDeselectOthers, or kNotChangeOthers (using cmd or ctrl),
  // then initialize the anchor OPTION to the clicked OPTION.
  if (!active_selection_anchor_ || mode != SelectionMode::kRange)
    SetActiveSelectionAnchor(clicked_option);

  SetActiveSelectionEnd(clicked_option);
  UpdateListBoxSelection(mode != SelectionMode::kNotChangeOthers);
}

void ListBoxSelectType::UpdateListBoxSelection(bool deselect_other_options,
                                               bool scroll) {
  DCHECK(select_->GetLayoutObject());
  HTMLOptionElement* const anchor_option = active_selection_anchor_;
  HTMLOptionElement* const end_option = active_selection_end_;
  const int anchor_index = anchor_option ? anchor_option->index() : -1;
  const int end_index = end_option ? end_option->index() : -1;
  const int start = std::min(anchor_index, end_index);
  const int end = std::max(anchor_index, end_index);

  int i = 0;
  for (auto* const option : select_->GetOptionList()) {
    if (option->IsDisabledFormControl() || !option->GetLayoutObject()) {
      ++i;
      continue;
    }
    if (i >= start && i <= end) {
      option->SetSelectedState(active_selection_state_);
      option->SetDirty(true);
    } else if (deselect_other_options ||
               i >= static_cast<int>(
                        cached_state_for_active_selection_.size())) {
      option->SetSelectedState(false);
      option->SetDirty(true);
    } else {
      option->SetSelectedState(cached_state_for_active_selection_[i]);
    }
    ++i;
  }

  UpdateMultiSelectFocus();
  select_->SetNeedsValidityCheck();
  if (scroll)
    ScrollToSelection();
  select_->NotifyFormStateChanged();
}

void ListBoxSelectType::SaveListboxActiveSelection() {
  // Cache the selection state so we can restore the old selection as the new
  // selection pivots around this anchor index.
  // Example:
  // 1. Press the mouse button on the second OPTION
  //   active_selection_anchor_ points the second OPTION.
  // 2. Drag the mouse pointer onto the fifth OPTION
  //   active_selection_end_ points the fifth OPTION, OPTIONs at 1-4 indices
  //   are selected.
  // 3. Drag the mouse pointer onto the fourth OPTION
  //   active_selection_end_ points the fourth OPTION, OPTIONs at 1-3 indices
  //   are selected.
  //   UpdateListBoxSelection needs to clear selection of the fifth OPTION.
  cached_state_for_active_selection_.resize(0);
  for (auto* const option : select_->GetOptionList()) {
    cached_state_for_active_selection_.push_back(option->Selected());
  }
}

void ListBoxSelectType::HandleMouseRelease() {
  // We didn't start this click/drag on any options.
  if (last_on_change_selection_.empty())
    return;
  ListBoxOnChange();
}

void ListBoxSelectType::ListBoxOnChange() {
  const auto& items = select_->GetListItems();

  // If the cached selection list is empty, or the size has changed, then fire
  // 'change' event, and return early.
  // FIXME: Why? This looks unreasonable.
  if (last_on_change_selection_.empty() ||
      last_on_change_selection_.size() != items.size()) {
    select_->DispatchChangeEvent();
    return;
  }

  // Update last_on_change_selection_ and fire a 'change' event.
  bool fire_on_change = false;
  for (unsigned i = 0; i < items.size(); ++i) {
    HTMLElement* element = items[i];
    auto* option_element = DynamicTo<HTMLOptionElement>(element);
    bool selected = option_element && option_element->Selected();
    if (selected != last_on_change_selection_[i])
      fire_on_change = true;
    last_on_change_selection_[i] = selected;
  }

  if (fire_on_change) {
    select_->DispatchInputEvent();
    select_->DispatchChangeEvent();
  }
}

void ListBoxSelectType::ClearLastOnChangeSelection() {
  last_on_change_selection_.clear();
}

void ListBoxSelectType::CreateShadowSubtree(ShadowRoot& root) {
  Document& doc = select_->GetDocument();
  option_slot_ = MakeGarbageCollected<HTMLSlotElement>(doc);
  option_slot_->SetIdAttribute(shadow_element_names::kSelectOptions);
  root.appendChild(option_slot_);
}

void ListBoxSelectType::ManuallyAssignSlots() {
  VectorOf<Node> option_nodes;
  for (Node& child : NodeTraversal::ChildrenOf(*select_)) {
    if (child.IsSlotable() && CanAssignToSelectSlot(child)) {
      option_nodes.push_back(child);
    }
  }
  option_slot_->Assign(option_nodes);
  if (RuntimeEnabledFeatures::StylableSelectEnabled()) {
    select_->GetShadowRoot()->SetDelegatesFocus(false);
  }
}

HTMLButtonElement* ListBoxSelectType::SlottedButton() const {
  return nullptr;
}

HTMLDataListElement* ListBoxSelectType::DisplayedDatalist() const {
  return nullptr;
}

bool ListBoxSelectType::IsAppearanceBaseSelect() const {
  return false;
}

// ============================================================================

SelectType::SelectType(HTMLSelectElement& select) : select_(select) {}

SelectType* SelectType::Create(HTMLSelectElement& select) {
  if (select.UsesMenuList())
    return MakeGarbageCollected<MenuListSelectType>(select);
  else
    return MakeGarbageCollected<ListBoxSelectType>(select);
}

void SelectType::WillBeDestroyed() {
  will_be_destroyed_ = true;
}

void SelectType::Trace(Visitor* visitor) const {
  visitor->Trace(select_);
}

void SelectType::OptionRemoved(HTMLOptionElement& option) {}

void SelectType::DidDetachLayoutTree() {}

void SelectType::DidRecalcStyle(const StyleRecalcChange) {}

void SelectType::UpdateTextStyle() {}

void SelectType::UpdateTextStyleAndContent() {}

HTMLOptionElement* SelectType::OptionToBeShown() const {
  NOTREACHED();
  return nullptr;
}

const ComputedStyle* SelectType::OptionStyle() const {
  NOTREACHED();
  return nullptr;
}

void SelectType::MaximumOptionWidthMightBeChanged() const {}

HTMLOptionElement* SelectType::SpatialNavigationFocusedOption() {
  return nullptr;
}

HTMLOptionElement* SelectType::ActiveSelectionEnd() const {
  NOTREACHED();
  return nullptr;
}

void SelectType::ScrollToSelection() {}

void SelectType::ScrollToOption(HTMLOptionElement* option) {}

void SelectType::SelectAll() {
  NOTREACHED();
}

void SelectType::SaveListboxActiveSelection() {}

void SelectType::HandleMouseRelease() {}

void SelectType::ListBoxOnChange() {}

void SelectType::ClearLastOnChangeSelection() {}

Element& SelectType::InnerElementForAppearanceAuto() const {
  NOTREACHED();
  // Returning select_ doesn't make sense, but we need to return an element
  // to compile this source. This function must not be called.
  return *select_;
}

void SelectType::ShowPicker() {}

void SelectType::ShowPopup(PopupMenu::ShowEventType) {
  NOTREACHED();
}

void SelectType::HidePopup() {
  NOTREACHED();
}

void SelectType::PopupDidHide() {
  NOTREACHED();
}

bool SelectType::PopupIsVisible() const {
  return false;
}

PopupMenu* SelectType::PopupForTesting() const {
  NOTREACHED();
  return nullptr;
}

AXObject* SelectType::PopupRootAXObject() const {
  NOTREACHED();
  return nullptr;
}

// Returns the 1st valid OPTION |skip| items from |list_index| in direction
// |direction| if there is one.
// Otherwise, it returns the valid OPTION closest to that boundary which is past
// |list_index| if there is one.
// Otherwise, it returns nullptr.
// Valid means that it is enabled and visible.
HTMLOptionElement* SelectType::NextValidOption(int list_index,
                                               SkipDirection direction,
                                               int skip) const {
  DCHECK(direction == kSkipBackwards || direction == kSkipForwards);
  const auto& list_items = select_->GetListItems();
  HTMLOptionElement* last_good_option = nullptr;
  int size = list_items.size();
  for (list_index += direction; list_index >= 0 && list_index < size;
       list_index += direction) {
    --skip;
    HTMLElement* element = list_items[list_index];
    auto* option_element = DynamicTo<HTMLOptionElement>(element);
    if (!option_element)
      continue;
    if (option_element->IsDisplayNone())
      continue;
    if (element->IsDisabledFormControl())
      continue;
    if (!select_->UsesMenuList() && !element->GetLayoutObject())
      continue;
    last_good_option = option_element;
    if (skip <= 0)
      break;
  }
  return last_good_option;
}

HTMLOptionElement* SelectType::NextSelectableOption(
    HTMLOptionElement* start_option) const {
  return NextValidOption(start_option ? start_option->ListIndex() : -1,
                         kSkipForwards, 1);
}

HTMLOptionElement* SelectType::PreviousSelectableOption(
    HTMLOptionElement* start_option) const {
  return NextValidOption(
      start_option ? start_option->ListIndex() : select_->GetListItems().size(),
      kSkipBackwards, 1);
}

HTMLOptionElement* SelectType::FirstSelectableOption() const {
  return NextValidOption(-1, kSkipForwards, 1);
}

HTMLOptionElement* SelectType::LastSelectableOption() const {
  return NextValidOption(select_->GetListItems().size(), kSkipBackwards, 1);
}

}  // namespace blink
