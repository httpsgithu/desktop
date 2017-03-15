// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testFocusLocationBar() {
    var firstFocusableNode = findAutomationNode(rootNode,
        function(node) {
          return node.role == 'textField' && node.state.focusable;
        });

    assertTrue(!!firstFocusableNode);
    listenOnce(firstFocusableNode, EventType.focus, function(e) {
      chrome.test.succeed();
    }, true);
    firstFocusableNode.focus();
  }
];

setUpAndRunTests(allTests);
