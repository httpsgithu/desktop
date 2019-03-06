// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by node BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class PaintPropertyNodeTest : public testing::Test {
 protected:
  template <typename NodeType>
  struct Tree {
    const NodeType* root;
    scoped_refptr<NodeType> ancestor;
    scoped_refptr<NodeType> child1;
    scoped_refptr<NodeType> child2;
    scoped_refptr<NodeType> grandchild1;
    scoped_refptr<NodeType> grandchild2;
  };

  void SetUp() override {
    //          root
    //           |
    //        ancestor
    //         /   \
    //     child1   child2
    //       |        |
    // grandchild1 grandchild2

    transform.root = &TransformPaintPropertyNode::Root();
    transform.ancestor =
        CreateTransform(*transform.root, TransformationMatrix());
    transform.child1 =
        CreateTransform(*transform.ancestor, TransformationMatrix());
    transform.child2 =
        CreateTransform(*transform.ancestor, TransformationMatrix());
    transform.grandchild1 =
        CreateTransform(*transform.child1, TransformationMatrix());
    transform.grandchild2 =
        CreateTransform(*transform.child2, TransformationMatrix());

    clip.root = &ClipPaintPropertyNode::Root();
    clip.ancestor =
        CreateClip(*clip.root, transform.ancestor.get(), FloatRoundedRect());
    clip.child1 =
        CreateClip(*clip.ancestor, transform.child1.get(), FloatRoundedRect());
    clip.child2 =
        CreateClip(*clip.ancestor, transform.child2.get(), FloatRoundedRect());
    clip.grandchild1 = CreateClip(*clip.child1, transform.grandchild1.get(),
                                  FloatRoundedRect());
    clip.grandchild2 = CreateClip(*clip.child2, transform.grandchild2.get(),
                                  FloatRoundedRect());

    effect.root = &EffectPaintPropertyNode::Root();
    effect.ancestor = CreateOpacityEffect(
        *effect.root, transform.ancestor.get(), clip.ancestor.get(), 0.5);
    effect.child1 = CreateOpacityEffect(
        *effect.ancestor, transform.child1.get(), clip.child1.get(), 0.5);
    effect.child2 = CreateOpacityEffect(
        *effect.ancestor, transform.child2.get(), clip.child2.get(), 0.5);
    effect.grandchild1 =
        CreateOpacityEffect(*effect.child1, transform.grandchild1.get(),
                            clip.grandchild1.get(), 0.5);
    effect.grandchild2 =
        CreateOpacityEffect(*effect.child2, transform.grandchild2.get(),
                            clip.grandchild2.get(), 0.5);
  }

  template <typename NodeType>
  void ResetAllChanged(Tree<NodeType>& tree) {
    tree.grandchild1->ClearChangedToRoot();
    tree.grandchild2->ClearChangedToRoot();
  }

  void ResetAllChanged() {
    ResetAllChanged(transform);
    ResetAllChanged(clip);
    ResetAllChanged(effect);
  }

  template <typename NodeType>
  void ExpectInitialState(const Tree<NodeType>& tree) {
    EXPECT_FALSE(tree.root->NodeChanged());
    EXPECT_TRUE(tree.ancestor->NodeChanged());
    EXPECT_TRUE(tree.child1->NodeChanged());
    EXPECT_TRUE(tree.child2->NodeChanged());
    EXPECT_TRUE(tree.grandchild1->NodeChanged());
    EXPECT_TRUE(tree.grandchild2->NodeChanged());
  }

  template <typename NodeType>
  void ExpectUnchangedState(const Tree<NodeType>& tree) {
    EXPECT_FALSE(tree.root->NodeChanged());
    EXPECT_FALSE(tree.ancestor->NodeChanged());
    EXPECT_FALSE(tree.child1->NodeChanged());
    EXPECT_FALSE(tree.child2->NodeChanged());
    EXPECT_FALSE(tree.grandchild1->NodeChanged());
    EXPECT_FALSE(tree.grandchild2->NodeChanged());
  }

  void ExpectUnchangedState() {
    ExpectUnchangedState(transform);
    ExpectUnchangedState(clip);
    ExpectUnchangedState(effect);
  }

  Tree<TransformPaintPropertyNode> transform;
  Tree<ClipPaintPropertyNode> clip;
  Tree<EffectPaintPropertyNode> effect;
};

#define STATE(node) \
  PropertyTreeState(&*transform.node, &*clip.node, &*effect.node)

TEST_F(PaintPropertyNodeTest, LowestCommonAncestor) {
  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.ancestor, *transform.ancestor));
  EXPECT_EQ(transform.root,
            &LowestCommonAncestor(*transform.root, *transform.root));

  EXPECT_EQ(transform.ancestor, &LowestCommonAncestor(*transform.grandchild1,
                                                      *transform.grandchild2));
  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.grandchild1, *transform.child2));
  EXPECT_EQ(transform.root,
            &LowestCommonAncestor(*transform.grandchild1, *transform.root));
  EXPECT_EQ(transform.child1,
            &LowestCommonAncestor(*transform.grandchild1, *transform.child1));

  EXPECT_EQ(transform.ancestor, &LowestCommonAncestor(*transform.grandchild2,
                                                      *transform.grandchild1));
  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.grandchild2, *transform.child1));
  EXPECT_EQ(transform.root,
            &LowestCommonAncestor(*transform.grandchild2, *transform.root));
  EXPECT_EQ(transform.child2,
            &LowestCommonAncestor(*transform.grandchild2, *transform.child2));

  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.child1, *transform.child2));
  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.child2, *transform.child1));
}

TEST_F(PaintPropertyNodeTest, InitialStateAndReset) {
  ExpectInitialState(transform);
  ResetAllChanged(transform);
  ExpectUnchangedState(transform);
}

TEST_F(PaintPropertyNodeTest, TransformChangeAncestor) {
  ResetAllChanged();
  ExpectUnchangedState();
  transform.ancestor->Update(*transform.root,
                             TransformPaintPropertyNode::State{
                                 TransformationMatrix().Translate(1, 2)});

  // Test descendant->Changed(ancestor).
  EXPECT_TRUE(transform.ancestor->Changed(*transform.root));
  EXPECT_FALSE(transform.ancestor->Changed(*transform.ancestor));
  EXPECT_TRUE(transform.child1->Changed(*transform.root));
  EXPECT_FALSE(transform.child1->Changed(*transform.ancestor));
  EXPECT_TRUE(transform.grandchild1->Changed(*transform.root));
  EXPECT_FALSE(transform.grandchild1->Changed(*transform.ancestor));

  // Test property->Changed(non-ancestor-property). Should combine the changed
  // flags of the two paths to the root.
  EXPECT_TRUE(transform.grandchild1->Changed(*transform.child2));
  EXPECT_TRUE(transform.grandchild1->Changed(*transform.grandchild2));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ClipChangeAncestor) {
  ResetAllChanged();
  ExpectUnchangedState();
  clip.ancestor->Update(
      *clip.root, ClipPaintPropertyNode::State{transform.ancestor.get(),
                                               FloatRoundedRect(1, 2, 3, 4)});

  // Test descendant->Changed(ancestor).
  EXPECT_TRUE(clip.ancestor->Changed(STATE(root), nullptr));
  EXPECT_FALSE(clip.ancestor->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.child1->Changed(STATE(root), nullptr));
  EXPECT_FALSE(clip.child1->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(STATE(root), nullptr));
  EXPECT_FALSE(clip.grandchild1->Changed(STATE(ancestor), nullptr));

  // Test property->Changed(non-ancestor-property).
  // Simply walk to the root.
  EXPECT_TRUE(clip.grandchild1->Changed(STATE(child2), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(STATE(grandchild2), nullptr));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectChangeAncestor) {
  ResetAllChanged();
  ExpectUnchangedState();
  EffectPaintPropertyNode::State state{transform.ancestor.get(),
                                       clip.ancestor.get()};
  state.opacity = 0.9;
  effect.ancestor->Update(*effect.root, std::move(state));

  // Test descendant->Changed(ancestor).
  EXPECT_TRUE(effect.ancestor->Changed(STATE(root), nullptr));
  EXPECT_FALSE(effect.ancestor->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(effect.child1->Changed(STATE(root), nullptr));
  EXPECT_FALSE(effect.child1->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(effect.grandchild1->Changed(STATE(root), nullptr));
  EXPECT_FALSE(effect.grandchild1->Changed(STATE(ancestor), nullptr));

  // Test property->Changed(non-ancestor-property).
  // Simply walk to the root.
  EXPECT_TRUE(effect.grandchild1->Changed(STATE(child2), nullptr));
  EXPECT_TRUE(effect.grandchild1->Changed(STATE(grandchild2), nullptr));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, TransformChangeOneChild) {
  ResetAllChanged();
  ExpectUnchangedState();
  transform.child1->Update(*transform.ancestor,
                           TransformPaintPropertyNode::State{
                               TransformationMatrix().Translate(1, 2)});

  // Test descendant->Changed(ancestor).
  EXPECT_FALSE(transform.ancestor->Changed(*transform.root));
  EXPECT_FALSE(transform.ancestor->Changed(*transform.ancestor));
  EXPECT_TRUE(transform.child1->Changed(*transform.root));
  EXPECT_TRUE(transform.child1->Changed(*transform.ancestor));
  EXPECT_TRUE(transform.grandchild1->Changed(*transform.ancestor));
  EXPECT_FALSE(transform.grandchild1->Changed(*transform.child1));
  EXPECT_FALSE(transform.child2->Changed(*transform.ancestor));
  EXPECT_FALSE(transform.grandchild2->Changed(*transform.ancestor));

  // Test property->Changed(non-ancestor-property). Need to combine the changed
  // flags of the two paths to the root.
  EXPECT_TRUE(transform.child2->Changed(*transform.child1));
  EXPECT_TRUE(transform.child1->Changed(*transform.child2));
  EXPECT_TRUE(transform.child2->Changed(*transform.grandchild1));
  EXPECT_TRUE(transform.child1->Changed(*transform.grandchild2));
  EXPECT_TRUE(transform.grandchild1->Changed(*transform.child2));
  EXPECT_TRUE(transform.grandchild1->Changed(*transform.grandchild2));
  EXPECT_TRUE(transform.grandchild2->Changed(*transform.child1));
  EXPECT_TRUE(transform.grandchild2->Changed(*transform.grandchild1));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ClipChangeOneChild) {
  ResetAllChanged();
  ExpectUnchangedState();
  clip.child1->Update(
      *clip.root, ClipPaintPropertyNode::State{transform.ancestor.get(),
                                               FloatRoundedRect(1, 2, 3, 4)});

  // Test descendant->Changed(ancestor).
  EXPECT_FALSE(clip.ancestor->Changed(STATE(root), nullptr));
  EXPECT_FALSE(clip.ancestor->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.child1->Changed(STATE(root), nullptr));
  EXPECT_TRUE(clip.child1->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(STATE(ancestor), nullptr));
  EXPECT_FALSE(clip.grandchild1->Changed(STATE(child1), nullptr));
  EXPECT_FALSE(clip.child2->Changed(STATE(ancestor), nullptr));
  EXPECT_FALSE(clip.grandchild2->Changed(STATE(ancestor), nullptr));

  // Test property->Changed(non-ancestor-property).
  // Simply walk to the root, regardless of relative_to_state's path.
  EXPECT_FALSE(clip.child2->Changed(STATE(child1), nullptr));
  EXPECT_TRUE(clip.child1->Changed(STATE(child2), nullptr));
  EXPECT_FALSE(clip.child2->Changed(STATE(grandchild1), nullptr));
  EXPECT_TRUE(clip.child1->Changed(STATE(grandchild2), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(STATE(child2), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(STATE(grandchild2), nullptr));
  EXPECT_FALSE(clip.grandchild2->Changed(STATE(child1), nullptr));
  EXPECT_FALSE(clip.grandchild2->Changed(STATE(grandchild1), nullptr));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectChangeOneChild) {
  ResetAllChanged();
  ExpectUnchangedState();
  EffectPaintPropertyNode::State state{transform.ancestor.get(),
                                       clip.ancestor.get()};
  state.opacity = 0.9;
  effect.child1->Update(*effect.root, std::move(state));

  // Test descendant->Changed(ancestor).
  EXPECT_FALSE(effect.ancestor->Changed(STATE(root), nullptr));
  EXPECT_FALSE(effect.ancestor->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(effect.child1->Changed(STATE(root), nullptr));
  EXPECT_TRUE(effect.child1->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(effect.grandchild1->Changed(STATE(ancestor), nullptr));
  EXPECT_FALSE(effect.grandchild1->Changed(STATE(child1), nullptr));
  EXPECT_FALSE(effect.child2->Changed(STATE(ancestor), nullptr));
  EXPECT_FALSE(effect.grandchild2->Changed(STATE(ancestor), nullptr));

  // Test property->Changed(non-ancestor-property).
  // Simply walk to the root, regardless of relative_to_state's path.
  EXPECT_FALSE(effect.child2->Changed(STATE(child1), nullptr));
  EXPECT_TRUE(effect.child1->Changed(STATE(child2), nullptr));
  EXPECT_FALSE(effect.child2->Changed(STATE(grandchild1), nullptr));
  EXPECT_TRUE(effect.child1->Changed(STATE(grandchild2), nullptr));
  EXPECT_TRUE(effect.grandchild1->Changed(STATE(child2), nullptr));
  EXPECT_TRUE(effect.grandchild1->Changed(STATE(grandchild2), nullptr));
  EXPECT_FALSE(effect.grandchild2->Changed(STATE(child1), nullptr));
  EXPECT_FALSE(effect.grandchild2->Changed(STATE(grandchild1), nullptr));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, TransformReparent) {
  ResetAllChanged();
  ExpectUnchangedState();
  transform.child1->Update(*transform.child2,
                           TransformPaintPropertyNode::State{
                               TransformationMatrix().Translate(1, 2)});
  EXPECT_FALSE(transform.ancestor->Changed(*transform.root));
  EXPECT_TRUE(transform.child1->Changed(*transform.ancestor));
  EXPECT_TRUE(transform.child1->Changed(*transform.child2));
  EXPECT_FALSE(transform.child2->Changed(*transform.ancestor));
  EXPECT_TRUE(transform.grandchild1->Changed(*transform.ancestor));
  EXPECT_FALSE(transform.grandchild1->Changed(*transform.child1));
  EXPECT_TRUE(transform.grandchild1->Changed(*transform.child2));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ClipLocalTransformSpaceChange) {
  ResetAllChanged();
  ExpectUnchangedState();
  transform.child1->Update(*transform.ancestor,
                           TransformPaintPropertyNode::State{
                               TransformationMatrix().Translate(1, 2)});

  EXPECT_FALSE(clip.ancestor->Changed(STATE(root), nullptr));
  EXPECT_FALSE(clip.ancestor->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.child1->Changed(STATE(root), nullptr));
  EXPECT_TRUE(clip.child1->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(STATE(ancestor), nullptr));
  EXPECT_FALSE(clip.grandchild1->Changed(STATE(child1), nullptr));

  // Test with transform_not_to_check.
  EXPECT_FALSE(clip.child1->Changed(STATE(root), transform.child1.get()));
  EXPECT_FALSE(clip.child1->Changed(STATE(ancestor), transform.child1.get()));
  EXPECT_TRUE(
      clip.grandchild1->Changed(STATE(ancestor), transform.child1.get()));
  EXPECT_TRUE(clip.child1->Changed(STATE(root), transform.ancestor.get()));
  EXPECT_TRUE(clip.child1->Changed(STATE(ancestor), transform.ancestor.get()));
  EXPECT_TRUE(
      clip.grandchild1->Changed(STATE(ancestor), transform.ancestor.get()));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectLocalTransformSpaceChange) {
  // Let effect.child1 have pixel-moving filter.
  EffectPaintPropertyNode::State state{transform.child1.get(),
                                       clip.child1.get()};
  state.filter.AppendBlurFilter(20);
  effect.child1->Update(*effect.ancestor, std::move(state));

  ResetAllChanged();
  ExpectUnchangedState();
  transform.ancestor->Update(*transform.root,
                             TransformPaintPropertyNode::State{
                                 TransformationMatrix().Translate(1, 2)});

  EXPECT_FALSE(effect.ancestor->Changed(STATE(root), nullptr));
  EXPECT_FALSE(effect.ancestor->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(effect.child1->Changed(STATE(root), nullptr));
  EXPECT_FALSE(effect.child1->Changed(STATE(ancestor), nullptr));
  EXPECT_TRUE(effect.grandchild1->Changed(STATE(root), nullptr));
  EXPECT_FALSE(effect.grandchild1->Changed(STATE(ancestor), nullptr));
  EXPECT_FALSE(effect.grandchild1->Changed(STATE(child1), nullptr));
  // Effects without self or ancestor pixel-moving filter are not affected by
  // change of LocalTransformSpace.
  EXPECT_FALSE(effect.child2->Changed(STATE(root), nullptr));
  EXPECT_FALSE(effect.grandchild2->Changed(STATE(root), nullptr));

  // Test with transform_not_to_check.
  EXPECT_FALSE(effect.child1->Changed(STATE(root), transform.child1.get()));
  EXPECT_TRUE(effect.child1->Changed(STATE(root), transform.ancestor.get()));

  ResetAllChanged();
  ExpectUnchangedState();
}

}  // namespace blink
