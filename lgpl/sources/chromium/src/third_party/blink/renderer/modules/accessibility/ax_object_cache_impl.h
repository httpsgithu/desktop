/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_

#include <memory>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom-blink.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache_base.h"
#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/aom/computed_accessible_node.h"
#include "third_party/blink/renderer/core/editing/commands/selection_for_undo_step.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/modules/accessibility/aria_notification.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/blink_ax_tree_source.h"
#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/weak_cell.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"
#include "ui/accessibility/ax_error_types.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace blink {

class AXRelationCache;
class AbstractInlineTextBox;
class HTMLAreaElement;
class WebLocalFrameClient;

// Describes a decision on whether to create an AXNodeObject with or without a
// LayoutObject, or to prune the AX subtree at that point. Only pseudo element
// descendants are missing DOM nodes.
enum AXObjectType { kPruneSubtree = 0, kCreateFromNode, kCreateFromLayout };

struct TextChangedOperation {
  TextChangedOperation()
      : start(0),
        end(0),
        start_anchor_id(0),
        end_anchor_id(0),
        op(ax::mojom::blink::Command::kNone) {}
  TextChangedOperation(int start_in,
                       int end_in,
                       AXID start_id_in,
                       AXID end_id_in,
                       ax::mojom::blink::Command op_in)
      : start(start_in),
        end(end_in),
        start_anchor_id(start_id_in),
        end_anchor_id(end_id_in),
        op(op_in) {}
  int start;
  int end;
  AXID start_anchor_id;
  AXID end_anchor_id;
  ax::mojom::blink::Command op;
};

// This class should only be used from inside the accessibility directory.
class MODULES_EXPORT AXObjectCacheImpl
    : public AXObjectCacheBase,
      public mojom::blink::PermissionObserver {
 public:
  static AXObjectCache* Create(Document&, const ui::AXMode&);

  AXObjectCacheImpl(Document&, const ui::AXMode&);

  AXObjectCacheImpl(const AXObjectCacheImpl&) = delete;
  AXObjectCacheImpl& operator=(const AXObjectCacheImpl&) = delete;

  ~AXObjectCacheImpl() override;
  void Trace(Visitor*) const override;

  // The main document.
  Document& GetDocument() const { return *document_; }
  // The popup document, if showing, otherwise null.
  Document* GetPopupDocumentIfShowing() const { return popup_document_.Get(); }

  AXObject* FocusedObject();
  const ui::AXMode& GetAXMode() override;
  void SetAXMode(const ui::AXMode&) override;

  // When the accessibility tree view is open in DevTools, we listen for changes
  // to the tree by registering an InspectorAccessibilityAgent here and notify
  // the agent when AXEvents are fired or nodes are marked dirty.
  void AddInspectorAgent(InspectorAccessibilityAgent*);
  void RemoveInspectorAgent(InspectorAccessibilityAgent*);

  // Ensure that a full document lifecycle will occur, which in turn ensures
  // that a call to ProcessDeferredAccessibilityEvents() will occur soon.
  void ScheduleAXUpdate() const override;

  void Dispose() override;

  // Freeze that AXObject tree and do not allow changes until Thaw() is called.
  // Prefer ScopedFreezeAXCache where possible.
  void Freeze() override {
    if (frozen_count_++) {
      // Already frozen.
      return;
    }
    ax_tree_source_->Freeze();
    CHECK(FocusedObject());
    DUMP_WILL_BE_CHECK(!IsDirty());
  }
  void Thaw() override {
    CHECK_GE(frozen_count_, 1);
    if (--frozen_count_ == 0) {
      ax_tree_source_->Thaw();
    }
  }
  bool IsFrozen() const override { return frozen_count_; }

  //
  // Iterators.
  //

  void SelectionChanged(Node*) override;

  // Uses the relation cache to check whether the current element is pointed to
  // by aria-labelledby or aria-describedby.
  bool IsLabelOrDescription(Element&);

  // Effects a ChildrenChanged() on the passed-in object, if unignored,
  // otherwise, uses the first unignored ancestor. Returns the object that the
  // children changed occurs on.
  AXObject* ChildrenChanged(AXObject*);
  void ChildrenChangedWithCleanLayout(AXObject*);
  void ChildrenChanged(Node*) override;
  void ChildrenChanged(AccessibleNode*) override;
  void ChildrenChanged(const LayoutObject*) override;
  void SlotAssignmentWillChange(Node*) override;
  void CheckedStateChanged(Node*) override;
  void ListboxOptionStateChanged(HTMLOptionElement*) override;
  void ListboxSelectedChildrenChanged(HTMLSelectElement*) override;
  void ListboxActiveIndexChanged(HTMLSelectElement*) override;
  void SetMenuListOptionsBounds(HTMLSelectElement*,
                                const WTF::Vector<gfx::Rect>&) override;
  void ImageLoaded(const LayoutObject*) override;

  // Removes AXObject backed by passed-in object, if there is one.
  // It will also notify the parent that its children have changed, so that the
  // parent will recompute its children and be reserialized.
  void Remove(AccessibleNode*) override;
  void Remove(Node*) override;
  void RemovePopup(Document*) override;
  void Remove(AbstractInlineTextBox*) override;
  // Remove an AXObject or its subtree, and if |notify_parent| is true,
  // recompute the parent's children and reserialize the parent.
  void Remove(AXObject*, bool notify_parent);
  void Remove(Node*, bool notify_parent);

  // This will remove all AXObjects in the subtree, whether they or not they are
  // marked as included for serialization. This can only be called while flat
  // tree traversal is safe and there are no slot assignments pending.
  // To remove only included nodes, use RemoveIncludedSubtree(), which can be
  // called at any time.
  // If |remove_root|, remove the root of the subtree, otherwise only
  // descendants are removed.
  // If |notify_parent|, call ChildrenChanged() on the parent.
  // If |only_layout_objects|, will only remove nodes in the subtree that
  // corresponded with an AXLayoutObject (useful for subtrees that lose layout).
  void RemoveSubtreeWithFlatTraversal(const Node*,
                                      bool remove_root = true,
                                      bool notify_parent = true);
  void RemoveSubtreeWhenSafe(Node*, bool remove_root = true) override;

  // Remove the cached subtree of included AXObjects. If |remove_root| is false,
  // then only descendants will be removed. To remove unincluded AXObjects as
  // well, call RemoveSubtreeWithFlatTraversal() or RemoveSubtreeWhenSafe().
  // If |remove_root|, remove the root of the subtree, otherwise only
  // descendants are removed.
  void RemoveIncludedSubtree(AXObject* object, bool remove_root);
  // Remove all AXObjects in the layout subtree of node, and notify the parent.
  void RemoveAXObjectsInLayoutSubtree(LayoutObject* layout_object) override;
  void RemoveAXObjectsInLayoutSubtree(Node* node) override;

  // For any ancestor that could contain the passed-in AXObject* in their cached
  // children, clear their children and set needs to update children on them.
  // In addition, ChildrenChanged() on an included ancestor that might contain
  // this child, if one exists.
  void ChildrenChangedOnAncestorOf(AXObject*);

  const Element* RootAXEditableElement(const Node*) override;

  // Called when aspects of the style (e.g. color, alignment) change.
  void StyleChanged(const LayoutObject*,
                    bool visibility_or_inertness_changed) override;

  // Called by a node when text or a text equivalent (e.g. alt) attribute is
  // changed.
  void TextChanged(const LayoutObject*) override;
  void TextChangedWithCleanLayout(Node* optional_node, AXObject*);

  void FocusableChangedWithCleanLayout(Node* node);
  void DocumentTitleChanged() override;

  // Returns true if we can immediately process tree updates for this node.
  // The main reason we cannot is lacking enough context to determine the
  // relevance of a whitespace node.
  bool IsReadyToProcessTreeUpdatesForNode(const Node*);
  // Called when a node is connected to the document.
  void NodeIsConnected(Node*) override;
  // Called when a node is attached to the layout tree.
  void NodeIsAttached(Node*) override;
  // Called when a subtree is attached to the layout tree because of
  // content-visibility or previously display:none content gaining layout.
  void SubtreeIsAttached(Node*) override;

  void HandleAttributeChanged(const QualifiedName& attr_name,
                              Element*) override;
  void HandleValidationMessageVisibilityChanged(Node* form_control) override;
  void HandleEventListenerAdded(Node& node,
                                const AtomicString& event_type) override;
  void HandleEventListenerRemoved(Node& node,
                                  const AtomicString& event_type) override;
  void HandleFocusedUIElementChanged(Element* old_focused_element,
                                     Element* new_focused_element) override;
  void HandleInitialFocus() override;
  void HandleTextFormControlChanged(Node*) override;
  void HandleEditableTextContentChanged(Node*) override;
  void HandleDeletionOrInsertionInTextField(
      const SelectionInDOMTree& changed_selection,
      bool is_deletion) override;
  void HandleTextMarkerDataAdded(Node* start, Node* end) override;
  void HandleValueChanged(Node*) override;
  void HandleUpdateActiveMenuOption(Node*) override;
  void DidShowMenuListPopup(LayoutObject*) override;
  void DidHideMenuListPopup(LayoutObject*) override;
  void HandleLoadStart(Document*) override;
  void HandleLoadComplete(Document*) override;
  void HandleClicked(Node*) override;
  void HandleAttributeChanged(const QualifiedName& attr_name,
                              AccessibleNode*) override;

  void HandleAriaNotification(const Node*,
                              const String&,
                              const AriaNotificationOptions*) override;

  // Returns the ARIA notifications associated to a given `AXObject` and
  // releases them from `aria_notifications_`. If there are no notifications
  // stored for the given object, returns an empty `AriaNotifications`.
  AriaNotifications RetrieveAriaNotifications(const AXObject*) override;

  void SetCanvasObjectBounds(HTMLCanvasElement*,
                             Element*,
                             const PhysicalRect&) override;

  void InlineTextBoxesUpdated(LayoutObject*) override;

  // Get the amount of time, in ms, that event processing should be deferred
  // in order to more efficiently batch changes.
  int GetDeferredEventsDelay() const;

  // Called during the accessibility lifecycle to refresh the AX tree.
  void ProcessDeferredAccessibilityEvents(Document&, bool force) override;
  // Remove AXObject subtrees (once flat tree traversal is safe).
  void ProcessSubtreeRemovals() override;

  // Called when a HTMLFrameOwnerElement (such as an iframe element) changes the
  // embedding token of its child frame.
  void EmbeddingTokenChanged(HTMLFrameOwnerElement*) override;

  // Called when the scroll offset changes.
  void HandleScrollPositionChanged(LayoutObject*) override;

  void HandleScrolledToAnchor(const Node* anchor_node) override;

  // Invalidates the bounding box, which can be later retrieved by
  // SerializeLocationChanges.
  void InvalidateBoundingBox(const LayoutObject*) override;

  void SetCachedBoundingBox(AXID id, const ui::AXRelativeBounds& bounds);

  const AtomicString& ComputedRoleForNode(Node*) override;
  String ComputedNameForNode(Node*) override;

  void OnTouchAccessibilityHover(const gfx::Point&) override;

  AXObject* ObjectFromAXID(AXID id) const override;
  AXObject* Root() override;

  // Create an AXObject, and do not check if a previous one exists.
  // Also, initialize the object and add it to maps for later retrieval.
  AXObject* CreateAndInit(Node*, LayoutObject*, AXObject* parent_if_known);
  // Used for objects without backing DOM nodes, layout objects, etc.
  AXObject* CreateAndInit(ax::mojom::blink::Role, AXObject* parent);

  // Note that these functions do NOT guarantee that an AXObject will
  // be created. For instance, not all HTMLElements can have an AXObject,
  // such as <head> or <script> tags.
  AXObject* GetOrCreate(AccessibleNode*, AXObject* parent);
  AXObject* GetOrCreate(LayoutObject*, AXObject* parent_if_known);
  AXObject* GetOrCreate(LayoutObject* layout_object);
  AXObject* GetOrCreate(const Node*, AXObject* parent_if_known) override;
  AXObject* GetOrCreate(Node*, AXObject* parent_if_known);
  AXObject* GetOrCreate(Node*);
  AXObject* GetOrCreate(const Node*);
  AXObject* GetOrCreate(AbstractInlineTextBox*, AXObject* parent_if_known);

  // Compute the included parent and its children, and then return
  // the AXObject for |child|.
  AXObject* RepairChildrenOfIncludedParent(Node* child);

  // Return an AXObject for the AccessibleNode. If the AccessibleNode is
  // attached to an element, will return the AXObject for that element instead.
  AXObject* Get(AccessibleNode*);
  AXObject* Get(AbstractInlineTextBox*);

  // Get an AXObject* backed by the passed-in DOM node.
  AXObject* Get(const Node*) override;

  // Get an AXObject* backed by the passed-in LayoutObject, or the
  // LayoutObject's DOM node, if that is available.
  // If |parent_for_repair| is provided, and the object had been detached from
  // its parent, it will be set as the new parent.
  AXObject* Get(const LayoutObject*, AXObject* parent_for_repair = nullptr);

  // Return true if the object is still part of the tree, meaning that ancestors
  // exist or can be repaired all the way to the root.
  bool IsStillInTree(AXObject*);

  void ChildrenChangedWithCleanLayout(Node* optional_node_for_relation_update,
                                      AXObject*);

  // Mark an object or subtree dirty, aka its properties have changed and it
  // needs to be reserialized. Use the |*WithCleanLayout| versions when layout
  // is already known to be clean.
  void MarkAXObjectDirty(AXObject*);

  void MarkAXObjectDirtyWithCleanLayout(AXObject*);

  void MarkAXSubtreeDirtyWithCleanLayout(AXObject*);
  void MarkSubtreeDirty(Node*);
  void NotifySubtreeDirty(AXObject* obj);

  // Set the parent of |child|. If no parent is possible, this means the child
  // can no longer be in the AXTree, so remove the child.
  AXObject* RestoreParentOrPrune(AXObject* child);
  AXObject* RestoreParentOrPruneWithCleanLayout(AXObject* child);

  // When an object is created or its id changes, this must be called so that
  // the relation cache is updated.
  void MaybeNewRelationTarget(Node& node, AXObject* obj);

  void HandleActiveDescendantChangedWithCleanLayout(Node*);
  void SectionOrRegionRoleMaybeChangedWithCleanLayout(Node*);
  void TableCellRoleMaybeChanged(Node* node);
  void HandleRoleMaybeChangedWithCleanLayout(Node*);
  void HandleRoleChangeWithCleanLayout(Node*);
  void HandleAriaExpandedChangeWithCleanLayout(Node*);
  void HandleAriaSelectedChangedWithCleanLayout(Node*);
  void HandleAriaPressedChangedWithCleanLayout(Node*);
  void HandleNodeLostFocusWithCleanLayout(Node*);
  void HandleNodeGainedFocusWithCleanLayout(Node*);
  void NodeIsAttachedWithCleanLayout(Node*);
  void DidShowMenuListPopupWithCleanLayout(Node*);
  void DidHideMenuListPopupWithCleanLayout(Node*);
  void HandleScrollPositionChangedWithCleanLayout(Node*);
  void HandleValidationMessageVisibilityChangedWithCleanLayout(const Node*);
  void HandleUpdateActiveMenuOptionWithCleanLayout(Node*);
  void HandleEditableTextContentChangedWithCleanLayout(Node*);
  void UpdateAriaOwnsWithCleanLayout(Node*);
  void UpdateTableRoleWithCleanLayout(Node*);

  AXID GenerateAXID() const override;

  void PostNotification(const LayoutObject*, ax::mojom::blink::Event);
  void PostNotification(Node*, ax::mojom::blink::Event);
  void PostNotification(AXObject*, ax::mojom::blink::Event);

  //
  // Aria-owns support.
  //

  // Returns true if the given object's position in the tree was due to
  // aria-owns.
  bool IsAriaOwned(const AXObject*) const;

  // Returns the parent of the given object due to aria-owns, if valid.
  AXObject* ValidatedAriaOwner(const AXObject*) const;

  // Given an object that has an aria-owns attribute, return the validated
  // set of aria-owned children.
  void ValidatedAriaOwnedChildren(const AXObject* owner,
                                  HeapVector<Member<AXObject>>& owned_children);

  // Given a <map> element, get the image currently associated with it, if any.
  AXObject* GetAXImageForMap(HTMLMapElement& map);

  // Adds |object| to |fixed_or_sticky_node_ids_| if it has a fixed or sticky
  // position.
  void AddToFixedOrStickyNodeList(const AXObject* object);

  bool MayHaveHTMLLabel(const HTMLElement& elem);

  // Synchronously returns whether or not we currently have permission to
  // call AOM event listeners.
  bool CanCallAOMEventListeners() const;

  // This is called when an accessibility event is triggered and there are
  // AOM event listeners registered that would have been called.
  // Asynchronously requests permission from the user. If permission is
  // granted, it only applies to the next event received.
  void RequestAOMEventListenerPermission();

  // For built-in HTML form validation messages.
  AXObject* ValidationMessageObjectIfInvalid();

  WebAXAutofillSuggestionAvailability GetAutofillSuggestionAvailability(
      AXID id) const;
  void SetAutofillSuggestionAvailability(
      AXID id,
      WebAXAutofillSuggestionAvailability suggestion_availability);

  // Plugin support. These could in (along with the tree source/serializer
  // fields) move to their own subclass of AXObject.
  void AddPluginTreeToUpdate(ui::AXTreeUpdate* update);
  ui::AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>*
  GetPluginTreeSource();
  void SetPluginTreeSource(
      ui::AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>*
          source);
  ui::AXTreeSerializer<const ui::AXNode*,
                       std::vector<const ui::AXNode*>,
                       ui::AXTreeUpdate*,
                       ui::AXTreeData*,
                       ui::AXNodeData>*
  GetPluginTreeSerializer();
  void ResetPluginTreeSerializer();
  void MarkPluginDescendantDirty(ui::AXNodeID node_id);

  std::pair<ax::mojom::blink::EventFrom, ax::mojom::blink::Action>
  active_event_from_data() const {
    return std::make_pair(active_event_from_, active_event_from_action_);
  }

  void set_active_event_from_data(
      const ax::mojom::blink::EventFrom event_from,
      const ax::mojom::blink::Action event_from_action) {
    active_event_from_ = event_from;
    active_event_from_action_ = event_from_action;
  }

  Element* GetActiveAriaModalDialog() const;

  static bool UseAXMenuList() { return use_ax_menu_list_; }
  static bool ShouldCreateAXMenuListFor(const Node*);
  static bool ShouldCreateAXMenuListOptionFor(const Node*);
  static bool IsRelevantPseudoElement(const Node& node);
  static bool IsRelevantPseudoElementDescendant(
      const LayoutObject& layout_object);
  static bool IsRelevantSlotElement(const HTMLSlotElement& slot);
  static Node* GetClosestNodeForLayoutObject(const LayoutObject* layout_object);

  // Token to return this token in the next IPC, so that RenderFrameHostImpl
  // can discard stale data, when the token does not match the expected token.
  std::optional<uint32_t> reset_token_;
  void SetSerializationResetToken(uint32_t token) override {
    reset_token_ = token;
  }

  // Retrieves a vector of all AXObjects whose bounding boxes may have changed
  // since the last query. Sends the resulting vector over mojo to the browser
  // process. Clears the vector so that the next time it's
  // called, it will only retrieve objects that have changed since now.
  void SerializeLocationChanges();

  bool SerializeEntireTree(
      size_t max_node_count,
      base::TimeDelta timeout,
      ui::AXTreeUpdate*,
      std::set<ui::AXSerializationErrorFlag>* out_error = nullptr) override;

  // Marks an object as dirty to be serialized in the next serialization.
  void AddDirtyObjectToSerializationQueue(
      AXObject* obj,
      ax::mojom::blink::EventFrom event_from =
          ax::mojom::blink::EventFrom::kNone,
      ax::mojom::blink::Action event_from_action =
          ax::mojom::blink::Action::kNone,
      const std::vector<ui::AXEventIntent>& event_intents = {}) override;

  void GetUpdatesAndEventsForSerialization(
      std::vector<ui::AXTreeUpdate>& updates,
      std::vector<ui::AXEvent>& events,
      bool& had_end_of_test_event,
      bool& had_load_complete_messages);

  void GetImagesToAnnotate(ui::AXTreeUpdate& updates,
                           std::vector<ui::AXNodeData*>& nodes) override;

  // The difference between this and IsDirty():
  // - IsDirty() means there are updates to be processed when layout becomes
  // clean, in order to have a complete representation in the tree structure.
  // - HasDirtyOirtyObjects() means there are updates ready to be sent
  // to the serializer.
  // TODO(accessibility) Differentiate naming -- there are too many kinds of
  // "dirty", which leads to confusion.
  bool HasDirtyObjects() const override { return !dirty_objects_.empty(); }
  bool IsDirty() override;

  // Set the id of the node to fetch image data for. Normally the content
  // of images is not part of the accessibility tree, but one node at a
  // time can be designated as the image data node, which will send the
  // contents of the image with each accessibility update until another
  // node is designated.
  void SetImageAsDataNodeId(AXID id, const gfx::Size& max_size) {
    image_data_node_id_ = id;
    max_image_data_size_ = max_size;
  }

  AXID image_data_node_id() { return image_data_node_id_; }
  const gfx::Size& max_image_data_size() { return max_image_data_size_; }

  static constexpr int kDataTableHeuristicMinRows = 20;

  // Updates the AX tree by walking from the root, calling AXObject::
  // UpdateChildrenIfNecessary on each AXObject for which NeedsUpdate is true.
  // This method is part of a11y-during-render, and in particular transitioning
  // to an eager (as opposed to lazy) AX tree update pattern. See
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1342801#c12 for more
  // details.
  void UpdateTreeIfNeeded();

  void UpdateAXForAllDocuments() override;
  void MarkDocumentDirty() override;
  void ResetSerializer() override;
  void MarkElementDirty(const Node*) override;
  void MarkElementDirtyWithCleanLayout(const Node*);

  // TODO(accessibility) Create an a11y lifecycle that encompasses these.
  // Layout is clean and the cache is processing callbacks.
  bool IsProcessingDeferredEvents() const {
    return processing_deferred_events_;
  }
  bool EntireDocumentIsDirty() const { return mark_all_dirty_; }
  // Returns true if UpdateTreeIfNeeded has been called and has not finished.
  bool UpdatingTree() { return updating_tree_; }
  // The document/cache are in the tear-down phase.
  bool HasBeenDisposed() const { return has_been_disposed_; }
  // Assert that tree is completely up-to-date.
  void CheckTreeIsUpdated();
  void CheckStyleIsComplete(Document& document) const;

  bool SerializeUpdatesAndEvents();

  // Returns the `TextChangedOperation` associated with the `id` from the
  // `text_operation_in_node_ids_` map, if `id` is in the map.
  WTF::Vector<TextChangedOperation>* GetFromTextOperationInNodeIdMap(AXID id);

  // Clears the map after each call, should be called after each serialization.
  void ClearTextOperationInNodeIdMap();

  // TODO(accessibility) Convert methods consuming this into members so that we
  // can remove this accessor method.
  HashMap<DOMNodeId, bool>& whitespace_ignored_map() {
    return whitespace_ignored_map_;
  }

  // Adds an event to the list of pending_events_ and mark the object as dirty
  // via AXObjectCache::AddDirtyObjectToSerializationQueue. If
  // immediate_serialization is set, it schedules a serialization to be done at
  // the next available time without delays.
  void AddEventToSerializationQueue(const ui::AXEvent& event,
                                    bool immediate_serialization) override;

  // Called from browser to RAI and then to AXCache to notify that a
  // serialization has arrived to Browser.
  void OnSerializationReceived() override;

  // Used by outside classes to determine if a serialization is in the process
  // or not.
  bool IsSerializationInFlight() const override;

  // Used by outside classes, mainly RenderAccessibilityImpl, to inform
  // AXObjectCacheImpl that a serialization was cancelled.
  void OnSerializationCancelled() override;

  // Used by outside classes, mainly RenderAccessibilityImpl, to inform
  // AXObjectCacheImpl that a serialization was sent.
  void OnSerializationStartSend() override;

  ComputedAccessibleNode* GetOrCreateComputedAccessibleNode(AXID) override;

#if DCHECK_IS_ON()
  // This is called after a node's included status changes, to update the
  // included_node_count_ which is used to debug tree mismatches between the the
  // AXObjectCache and AXTreeSerializer.
  void UpdateIncludedNodeCount(const AXObject* obj);
  size_t GetIncludedNodeCount() const { return included_node_count_; }
  void UpdatePluginIncludedNodeCount();
  size_t GetPluginIncludedNodeCount() const {
    return plugin_included_node_count_;
  }
  HeapHashMap<AXID, Member<AXObject>>& GetObjects() { return objects_; }
#endif

 protected:
  void ScheduleImmediateSerialization() override;

  void PostPlatformNotification(
      AXObject* obj,
      ax::mojom::blink::Event event_type,
      ax::mojom::blink::EventFrom event_from =
          ax::mojom::blink::EventFrom::kNone,
      ax::mojom::blink::Action event_from_action =
          ax::mojom::blink::Action::kNone,
      const BlinkAXEventIntentsSet& event_intents = BlinkAXEventIntentsSet());
  void IdChangedWithCleanLayout(Node*);
  void AriaOwnsChangedWithCleanLayout(Node*);

  // Returns a reference to the set of currently active event intents.
  BlinkAXEventIntentsSet& ActiveEventIntents() override {
    return active_event_intents_;
  }

 private:
  struct AXDirtyObject : public GarbageCollected<AXDirtyObject> {
    AXDirtyObject(AXObject* obj_arg,
                  ax::mojom::blink::EventFrom event_from_arg,
                  ax::mojom::blink::Action event_from_action_arg,
                  std::vector<ui::AXEventIntent> event_intents_arg)
        : obj(obj_arg),
          event_from(event_from_arg),
          event_from_action(event_from_action_arg),
          event_intents(event_intents_arg) {}

    static AXDirtyObject* Create(AXObject* obj,
                                 ax::mojom::blink::EventFrom event_from,
                                 ax::mojom::blink::Action event_from_action,
                                 std::vector<ui::AXEventIntent> event_intents) {
      return MakeGarbageCollected<AXDirtyObject>(
          obj, event_from, event_from_action, event_intents);
    }

    void Trace(Visitor* visitor) const { visitor->Trace(obj); }

    Member<AXObject> obj;
    ax::mojom::blink::EventFrom event_from;
    ax::mojom::blink::Action event_from_action;
    std::vector<ui::AXEventIntent> event_intents ALLOW_DISCOURAGED_TYPE(
        "Avoids conversion when passed from/to ui::AXTreeUpdate or "
        "blink::WebAXObject");
  };

  // Make sure a relation cache exists and is initialized. Must be called with
  // clean layout.
  void EnsureRelationCache();

  // Make sure the AXTreeSerializer has been created.
  void EnsureSerializer();

  // Helpers for CreateAndInit().
  AXObject* CreateFromRenderer(LayoutObject*);
  AXObject* CreateFromNode(Node*);
  AXObject* CreateFromInlineTextBox(AbstractInlineTextBox*);

  // Removes AXObject backed by passed-in object, if there is one.
  // It will also notify the parent that its children have changed, so that the
  // parent will recompute its children and be reserialized, unless
  // |notify_parent| is passed in as false.
  void Remove(AccessibleNode*, bool notify_parent);
  void Remove(LayoutObject*, bool notify_parent);
  void Remove(AbstractInlineTextBox*, bool notify_parent);

  // Helper to remove the object from the cache.
  // Most callers should be using Remove(AXObject) instead.
  void Remove(AXID, bool notify_parent);
  // Helper to clean up any references to the AXObject's AXID.
  void RemoveReferencesToAXID(AXID);

  HeapMojoRemote<mojom::blink::RenderAccessibilityHost>&
  GetOrCreateRemoteRenderAccessibilityHost();
  WebLocalFrameClient* GetWebLocalFrameClient() const;
  void ProcessDeferredAccessibilityEventsImpl(Document&);
  void UpdateLifecycleIfNeeded(Document& document);

  // Is the main document currently parsing content, as opposed to being blocked
  // by script execution or being load complete state.
  bool IsParsingMainDocument() const;

  bool IsMainDocumentDirty() const;
  bool IsPopupDocumentDirty() const;
  void ProcessSubtreeRemoval(Node*, bool remove_root);

  // Returns true if the AXID is for a DOM node.
  // All other AXIDs are generated.
  bool IsDOMNodeID(AXID axid) { return axid > 0; }

  HeapHashSet<WeakMember<InspectorAccessibilityAgent>> agents_;

  struct AXEventParams final : public GarbageCollected<AXEventParams> {
    AXEventParams(AXObject* target,
                  ax::mojom::blink::Event event_type,
                  ax::mojom::blink::EventFrom event_from,
                  ax::mojom::blink::Action event_from_action,
                  const BlinkAXEventIntentsSet& intents)
        : target(target),
          event_type(event_type),
          event_from(event_from),
          event_from_action(event_from_action) {
      for (const auto& intent : intents) {
        event_intents.insert(intent.key, intent.value);
      }
    }
    Member<AXObject> target;
    ax::mojom::blink::Event event_type;
    ax::mojom::blink::EventFrom event_from;
    ax::mojom::blink::Action event_from_action;
    BlinkAXEventIntentsSet event_intents;

    void Trace(Visitor* visitor) const { visitor->Trace(target); }
  };

  // The following represent functions that could be used as callbacks for
  // DeferTreeUpdate. Every enum value represents a function that would be
  // called after a tree update is complete.
  // Please don't reuse these enums in multiple callers to DeferTreeUpdate().
  // Instead, add an enum where the suffix describes where it's being called
  // from (this helps when debugging an issue apparent in clean layout, by
  // helping clarify the code paths).
  enum class TreeUpdateReason : uint8_t {
    // These updates are always associated with a DOM Node:
    kActiveDescendantChanged = 1,
    kAriaExpandedChanged = 2,
    kAriaOwnsChanged = 3,
    kAriaPressedChanged = 4,
    kAriaSelectedChanged = 5,
    kDidHideMenuListPopup = 6,
    kDidShowMenuListPopup = 7,
    kEditableTextContentChanged = 8,
    kFocusableChanged = 9,
    kIdChanged = 10,
    kMarkDirtyFromHandleScroll = 12,
    kNodeGainedFocus = 13,
    kNodeLostFocus = 14,
    kPostNotificationFromHandleLoadComplete = 15,
    kPostNotificationFromHandleLoadStart = 16,
    kPostNotificationFromHandleScrolledToAnchor = 17,
    kRemoveValidationMessageObjectFromFocusedUIElement = 18,
    kRemoveValidationMessageObjectFromValidationMessageObject = 19,
    kRoleChangeFromAriaHasPopup = 20,
    kRoleChangeFromImageMapName = 21,
    kRoleChangeFromRoleOrType = 22,
    kRoleMaybeChangedFromEventListener = 23,
    kRoleMaybeChangedFromHref = 24,
    kSectionOrRegionRoleMaybeChangedFromLabel = 25,
    kSectionOrRegionRoleMaybeChangedFromLabelledBy = 26,
    kSectionOrRegionRoleMaybeChangedFromTitle = 27,
    kTextChangedOnNode = 28,
    kTextChangedOnClosestNodeForLayoutObject = 29,
    kTextMarkerDataAdded = 30,
    kUpdateActiveMenuOption = 31,
    kNodeIsAttached = 32,
    kUpdateAriaOwns = 33,
    kUpdateTableRole = 34,
    kUseMapAttributeChanged = 35,
    kValidationMessageVisibilityChanged = 36,

    // These updates are associated with an AXID:
    kChildrenChanged = 100,
    kMarkAXObjectDirty = 101,
    kMarkAXSubtreeDirty = 102,
    kTextChangedOnLayoutObject = 103
  };

  struct TreeUpdateParams final : public GarbageCollected<TreeUpdateParams> {
    TreeUpdateParams(
        Node* node_arg,
        AXID axid_arg,
        ax::mojom::blink::EventFrom event_from_arg,
        ax::mojom::blink::Action event_from_action_arg,
        const BlinkAXEventIntentsSet& intents_arg,
        TreeUpdateReason update_reason_arg,
        ax::mojom::blink::Event event_arg = ax::mojom::blink::Event::kNone)
        : node(node_arg),
          axid(axid_arg),
          event(event_arg),
          event_from(event_from_arg),
          update_reason(update_reason_arg),
          event_from_action(event_from_action_arg) {
      for (const auto& intent : intents_arg) {
        DCHECK(node || axid) << "Either a DOM Node or AXID is required.";
        DCHECK(!node || !axid) << "Provide a DOM Node *or* AXID, not both.";
        event_intents.insert(intent.key, intent.value);
      }
    }

    // Only either node or AXID will be filled at a time. Some events use Node
    // while others use AXObject.
    WeakMember<Node> node;
    AXID axid;

    ax::mojom::blink::Event event;
    ax::mojom::blink::EventFrom event_from;
    TreeUpdateReason update_reason;
    ax::mojom::blink::Action event_from_action;
    BlinkAXEventIntentsSet event_intents;

    virtual ~TreeUpdateParams() = default;
    void Trace(Visitor* visitor) const { visitor->Trace(node); }
  };

  typedef HeapVector<Member<TreeUpdateParams>> TreeUpdateCallbackQueue;

  bool IsImmediateProcessingRequired(TreeUpdateParams* tree_update) const;
  bool IsImmediateProcessingRequiredForEvent(AXEventParams* event) const;

  ax::mojom::blink::EventFrom ComputeEventFrom();

  void MarkAXObjectDirtyWithCleanLayoutHelper(
      AXObject* obj,
      ax::mojom::blink::EventFrom event_from,
      ax::mojom::blink::Action event_from_action);
  void MarkAXSubtreeDirty(AXObject*);
  void MarkDocumentDirtyWithCleanLayout();

  // Given an object to mark dirty or fire an event on, return an object
  // included in the tree that can be used with the serializer, or null if there
  // is no relevant object to use. Objects that are not included in the tree,
  // and have no ancestor object included in the tree, are pruned from the tree,
  // in which case there is nothing to be serialized.
  AXObject* GetSerializationTarget(AXObject* obj);

  // Helper that clears children up to the first included ancestor and returns
  // the ancestor if a children changed notification should be fired on it.
  AXObject* InvalidateChildren(AXObject* obj);

  Member<Document> document_;
  // Any popup document except for the popup for <select size=1>.
  Member<Document> popup_document_;

  ui::AXMode ax_mode_;

  // AXIDs for AXNodeObjects reuse the int ids in dom_node_id, all other AXIDs
  // are negative in order to avoid a conflict.
  HeapHashMap<AXID, Member<AXObject>> objects_;
  HeapHashMap<Member<AccessibleNode>, AXID> accessible_node_mapping_;
  // When the AXObject is backed by layout, its AXID can be looked up in
  // layout_object_mapping_. When the AXObject is backed by a node, its
  // AXID can be looked up via node->GetDomNodeId().
  HeapHashMap<Member<const LayoutObject>, AXID> layout_object_mapping_;
  HeapHashMap<Member<AbstractInlineTextBox>, AXID>
      inline_text_box_object_mapping_;
#if DCHECK_IS_ON()
  size_t included_node_count_ = 0;
  size_t plugin_included_node_count_ = 0;
#endif

  // Used for a mock AXObject representing the message displayed in the
  // validation message bubble.
  // There can be only one of these per document with invalid form controls,
  // and it will always be related to the currently focused control.
  AXID validation_message_axid_;

  // The currently active aria-modal dialog element, if one has been computed,
  // null if otherwise. This is only ever computed on platforms that have the
  // AriaModalPrunesAXTree setting enabled, such as Mac.
  WeakMember<Element> active_aria_modal_dialog_;

  // If non-null, this is the node that the current aria-activedescendant caused
  // to have the selected state.
  WeakMember<Node> last_selected_from_active_descendant_;
  // If non-zero, this is the DOMNodeID for the last <option> element selected
  // in a select with size > 1.
  DOMNodeId last_selected_list_option_ = 0;

  std::unique_ptr<AXRelationCache> relation_cache_;

  // Stages of cache/tree.
  // If all of these are false, the cache can collect updates to-be-processed
  // via callbacks from DOM/layout.
  // TODO(accessibility) Replace these with something like a document lifecycle.
  // Tree is being updated.
  bool processing_deferred_events_ = false;
  // If > 0, tree is frozen and beign serialized.
  int frozen_count_ = 0;  // Used with Freeze(), Thaw() and IsFrozen() above.
  // Tree and cache are being destroyed.
  bool has_been_disposed_ = false;

#if DCHECK_IS_ON()
  bool updating_layout_and_ax_ = false;
  int tree_check_counter_ = 0;
  base::Time last_tree_check_time_stamp_ = base::Time::Now();
#endif

  // If non-zero, do not do work to process a11y or build the a11y tree in
  // ProcessDeferredAccessibilityEvents(). Will be set to 0 when more content
  // is loaded or the load is completed.
  size_t allowed_tree_update_pauses_remaining_ = 0;
  // If null, then any new connected node will unpause tree updates.
  // Otherwise, tree updates will unpause once the node is fully parsed.
  WeakMember<Node> node_to_parse_before_more_tree_updates_;

  HeapVector<Member<AXEventParams>> notifications_to_post_main_;
  HeapVector<Member<AXEventParams>> notifications_to_post_popup_;

  // Call the queued callback methods that do processing which must occur when
  // layout is clean. These callbacks are stored in tree_update_callback_queue_,
  // and have names like FooBarredWithCleanLayout().
  void ProcessCleanLayoutCallbacks(Document&);

  // Send events to RenderAccessibilityImpl, which serializes them and then
  // sends the serialized events and dirty objects to the browser process.
  void PostNotifications(Document&);

  // Get the currently focused Node (an element or a document).
  Node* FocusedNode();

  AXObject* FocusedImageMapUIElement(HTMLAreaElement*);

  // Associate an AXObject with an AXID. Generate one if none is supplied.
  AXID AssociateAXID(AXObject*, AXID use_axid = 0);

  void TextChanged(Node*);
  bool NodeIsTextControl(const Node*);
  AXObject* NearestExistingAncestor(Node*);

  Settings* GetSettings();

  // Start listenening for updates to the AOM accessibility event permission.
  void AddPermissionStatusListener();

  // mojom::blink::PermissionObserver implementation.
  // Called when we get an updated AOM event listener permission value from
  // the browser.
  void OnPermissionStatusChange(mojom::PermissionStatus) override;

  // When a <tr> or <td> is inserted or removed, the containing table may have
  // gained or lost rows or columns.
  void ContainingTableRowsOrColsMaybeChanged(Node*);

  // Object for HTML validation alerts. Created at most once per object cache.
  AXObject* GetOrCreateValidationMessageObject();
  void RemoveValidationMessageObjectWithCleanLayout(Node* document);

  // To be called inside DeferTreeUpdate to check the queue status before
  // adding.
  bool CanDeferTreeUpdate(Document* tree_update_document);

  // Checks the update queue, then pauses and rebuilds it if full. Returns true
  // of the queue was paused.
  bool PauseTreeUpdatesIfQueueFull();

  // Enqueue a callback to the given method to be run after layout is
  // complete.
  void DeferTreeUpdate(
      AXObjectCacheImpl::TreeUpdateReason update_reason,
      Node* node,
      ax::mojom::blink::Event event = ax::mojom::blink::Event::kNone);

  // Provide either a DOM node or AXObject. If both are provided, then they must
  // match, meaning that the AXObject's DOM node must equal the provided node.
  void DeferTreeUpdate(
      AXObjectCacheImpl::TreeUpdateReason update_reason,
      AXObject* obj,
      ax::mojom::blink::Event event = ax::mojom::blink::Event::kNone);

  void TextChangedWithCleanLayout(Node* node);
  void ChildrenChangedWithCleanLayout(Node* node);

  // If the presence of document markers changed for the given text node, then
  // call children changed.
  void HandleTextMarkerDataAddedWithCleanLayout(Node*);
  void HandleUseMapAttributeChangedWithCleanLayout(Node*);
  void HandleNameAttributeChanged(Node*);

  bool DoesEventListenerImpactIgnoredState(const AtomicString& event_type,
                                           const Node& node) const;
  void HandleEventSubscriptionChanged(Node& node,
                                      const AtomicString& event_type);

  //
  // aria-modal support
  //

  // This function is only ever called on platforms where the
  // AriaModalPrunesAXTree setting is enabled, and the accessibility tree must
  // be manually pruned to remove background content.
  void UpdateActiveAriaModalDialog(Node* element);

  // This will return null on platforms without the AriaModalPrunesAXTree
  // setting enabled, or where there is no active ancestral aria-modal dialog.
  Element* AncestorAriaModalDialog(Node* node);

  // Return the AXObject for the update if it is relevant (its backing data has
  // not been destroyed and it is attached to the expected tree document).
  AXObject* TreeUpdateObjectIfRelevant(Document& document,
                                       TreeUpdateParams* tree_update);

  void FireTreeUpdatedEventImmediately(TreeUpdateParams* tree_update,
                                       AXObject* ax_object);

  void FireAXEventImmediately(AXObject* obj,
                              ax::mojom::blink::Event event_type,
                              ax::mojom::blink::EventFrom event_from,
                              ax::mojom::blink::Action event_from_action,
                              const BlinkAXEventIntentsSet& event_intents);

  void SetMaxPendingUpdatesForTesting(wtf_size_t max_pending_updates) {
    max_pending_updates_ = max_pending_updates;
  }

  void UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

  // Invalidates the bounding boxes of fixed or sticky positioned objects which
  // should be updated when the scroll offset is changed. Like
  // InvalidateBoundingBox, it can be later retrieved by
  // SerializeLocationChanges.
  void InvalidateBoundingBoxForFixedOrStickyPosition();

  // Return true if this is the popup document. There can only be one popup
  // document at a time. If it is not the popup document, it's the main
  // document stored in |document_|.
  bool IsPopup(Document& document) const;

  // Get the queued tree update callbacks for the passed-in document
  TreeUpdateCallbackQueue& GetTreeUpdateCallbackQueue(Document& document);

  // Get the event notifications to post for the passed-in document.
  HeapVector<Member<AXEventParams>>& GetNotificationsToPost(Document& document);

  // Whether the user has granted permission for the user to install event
  // listeners for accessibility events using the AOM.
  mojom::PermissionStatus accessibility_event_permission_;
  // The permission service, enabling us to check for event listener
  // permission.
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
  HeapMojoReceiver<mojom::blink::PermissionObserver, AXObjectCacheImpl>
      permission_observer_receiver_;

  // Queued callbacks.
  TreeUpdateCallbackQueue tree_update_callback_queue_main_;
  TreeUpdateCallbackQueue tree_update_callback_queue_popup_;

  // Help de-dupe processing of repetitive events.
  HashSet<AXID> nodes_with_pending_children_changed_;

  // Nodes with document markers that have received accessibility updates.
  HashSet<AXID> nodes_with_spelling_or_grammar_markers_;

  // Nodes renoved from flat tree.
  HeapVector<std::pair<Member<Node>, bool>> nodes_for_subtree_removal_;

  AXID last_value_change_node_ = ui::AXNodeData::kInvalidAXID;

  // If tree_update_callback_queue_ gets improbably large, stop
  // enqueueing updates and fire a single ChildrenChanged event on the
  // document once layout occurs.
  wtf_size_t max_pending_updates_ = 1UL << 16;
  bool tree_updates_paused_ = false;

  // This will flip to true when we initiate the process of sending AX data to
  // the browser, and will flip back to false once we receive back an ACK.
  bool serialization_in_flight_ = false;

  // This stores the last time a serialization was ACK'ed after being sent to
  // the browser, so that serializations can be skipped if the time since the
  // last serialization is less than GetDeferredEventsDelay(). Setting to
  // "beginning of time" causes the upcoming serialization to occur at the next
  // available opportunity.  Batching is used to reduce the number of
  // serializations, in order to provide overall faster content updates while
  // using less CPU, because nodes that change multiple times in a short time
  // period only need to be serialized once, e.g. during page loads or
  // animations.
  base::Time last_serialization_timestamp_ = base::Time::UnixEpoch();

  // If true, will not attempt to batch and will serialize at the next
  // opportunity.
  bool serialize_immediately_ = false;

  // This flips to true if a request for an immediate update was not honored
  // because serialization_in_flight_ was true. It flips back to false once
  // serialization_in_flight_ has flipped to false and an immediate update has
  // been requested.
  bool serialize_immediately_after_current_serialization_ = false;

  // Maps ids to their object's autofill suggestion availability.
  HashMap<AXID, WebAXAutofillSuggestionAvailability>
      autofill_suggestion_availability_map_;

  // The set of node IDs whose bounds has changed since the last time
  // SerializeLocationChanges was called.
  HashSet<AXID> changed_bounds_ids_;

  // Known locations and sizes of bounding boxes that are known to have been
  // serialized.
  HashMap<AXID, ui::AXRelativeBounds> cached_bounding_boxes_;

  // The list of node IDs whose position is fixed or sticky.
  HashSet<AXID> fixed_or_sticky_node_ids_;

  // Map of node IDs where there was an operation done, could be deletion or
  // insertion. The items in the vector are in the order that the operations
  // were made in.
  HashMap<AXID, WTF::Vector<TextChangedOperation>> text_operation_in_node_ids_;

  // Used to keep track of which ComputedAccessibleNodes have already been
  // instantiated in this document to avoid constructing duplicates.
  HeapHashMap<AXID, Member<ComputedAccessibleNode>> computed_node_mapping_;

  // A set of ARIA notifications that have yet to be added to `ax_tree_data`.
  HashMap<AXID, AriaNotifications> aria_notifications_;

  // The source of the event that is currently being handled.
  ax::mojom::blink::EventFrom active_event_from_ =
      ax::mojom::blink::EventFrom::kNone;

  // The accessibility action that caused the event. Will only be valid if
  // active_event_from_ is set to kAction.
  ax::mojom::blink::Action active_event_from_action_ =
      ax::mojom::blink::Action::kNone;

  // A set of currently active event intents.
  BlinkAXEventIntentsSet active_event_intents_;

  // If false, exposes the internal accessibility tree of a select pop-up
  // instead.
  static bool use_ax_menu_list_;

  HeapMojoRemote<mojom::blink::RenderAccessibilityHost>
      render_accessibility_host_;

  Member<BlinkAXTreeSource> ax_tree_source_;
  std::unique_ptr<ui::AXTreeSerializer<AXObject*,
                                       HeapVector<Member<AXObject>>,
                                       ui::AXTreeUpdate*,
                                       ui::AXTreeData*,
                                       ui::AXNodeData>>
      ax_tree_serializer_;

  HeapVector<Member<AXDirtyObject>> dirty_objects_;

  Vector<ui::AXEvent> pending_events_;

  HashMap<DOMNodeId, bool> whitespace_ignored_map_;

  bool updating_tree_ = false;
  // Make sure the next serialization sends everything.
  bool mark_all_dirty_ = false;

  mutable bool has_axid_generator_looped_ = false;

  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest,
                           UpdateAXForAllDocumentsAfterPausedUpdates);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, RemoveReferencesToAXID);

  // The ID of the object to fetch image data for.
  AXID image_data_node_id_ = ui::AXNodeData::kInvalidAXID;

  gfx::Size max_image_data_size_;

  using PluginAXTreeSerializer =
      ui::AXTreeSerializer<const ui::AXNode*,
                           std::vector<const ui::AXNode*>,
                           ui::AXTreeUpdate*,
                           ui::AXTreeData*,
                           ui::AXNodeData>;
  // AXTreeSerializer's AXSourceNodeVectorType is not a vector<raw_ptr> due to
  // performance regressions detected in blink_perf.accessibility tests.
  RAW_PTR_EXCLUSION std::unique_ptr<PluginAXTreeSerializer> plugin_serializer_;
  raw_ptr<ui::AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>>
      plugin_tree_source_;

  // So we can ensure the serialization pipeline never stalls with dirty objects
  // remaining to be serialized.
  blink::WeakCellFactory<AXObjectCacheImpl>
      weak_factory_for_serialization_pipeline_{this};
};

// This is the only subclass of AXObjectCache.
template <>
struct DowncastTraits<AXObjectCacheImpl> {
  static bool AllowFrom(const AXObjectCache& cache) { return true; }
};

// This will let you know if aria-hidden was explicitly set to false.
bool IsNodeAriaVisible(Node*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_
