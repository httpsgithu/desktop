// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';

import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-expand-button', function() {
  let button: CrExpandButtonElement;
  let icon: CrIconButtonElement;
  const expandTitle = 'expand title';
  const collapseTitle = 'collapse title';

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    button = document.createElement('cr-expand-button');
    document.body.appendChild(button);
    icon = button.$.icon;
    await button.updateComplete;
  });

  test('setting |aria-label| label', async () => {
    assertFalse(!!button.ariaLabel);
    assertEquals('label', icon.getAttribute('aria-labelledby'));
    assertEquals(null, icon.getAttribute('aria-label'));
    const ariaLabel = 'aria-label label';
    button.ariaLabel = ariaLabel;
    await button.updateComplete;
    assertEquals(null, icon.getAttribute('aria-labelledby'));
    assertEquals(ariaLabel, icon.getAttribute('aria-label'));
  });

  test('changing |expanded|', async () => {
    button.expandTitle = expandTitle;
    button.collapseTitle = collapseTitle;
    await button.updateComplete;
    assertFalse(button.expanded);
    assertEquals(expandTitle, button.title);
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertEquals('cr:expand-more', icon.ironIcon);
    button.expanded = true;
    await button.updateComplete;
    assertEquals(collapseTitle, button.title);
    assertEquals('true', icon.getAttribute('aria-expanded'));
    assertEquals('cr:expand-less', icon.ironIcon);
  });

  test('expanded-changed event fires', async () => {
    let whenFired = eventToPromise('expanded-changed', button);
    button.expanded = true;
    let event = await whenFired;
    assertTrue(event.detail.value);

    whenFired = eventToPromise('expanded-changed', button);
    button.expanded = false;
    event = await whenFired;
    assertFalse(event.detail.value);
  });

  test('changing |disabled|', async () => {
    assertFalse(button.disabled);
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertFalse(icon.disabled);
    button.disabled = true;
    await button.updateComplete;
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertTrue(icon.disabled);
  });

  // Ensure that the label is marked with aria-hidden="true", so that screen
  // reader focus goes straight to the cr-icon-button.
  test('label aria-hidden', () => {
    const labelId = 'label';
    assertEquals(
        'true',
        button.shadowRoot!.querySelector(`#${labelId}`)!.getAttribute(
            'aria-hidden'));
    assertEquals(labelId, icon.getAttribute('aria-labelledby'));
  });

  test('setting |expand-icon| and |collapse-icon|', async () => {
    const expandIconName = 'cr:arrow-drop-down';
    button.setAttribute('expand-icon', expandIconName);
    const collapseIconName = 'cr:arrow-drop-up';
    button.setAttribute('collapse-icon', collapseIconName);
    await button.updateComplete;

    assertFalse(button.expanded);
    assertEquals(expandIconName, icon.ironIcon);
    button.expanded = true;
    await button.updateComplete;
    assertEquals(collapseIconName, icon.ironIcon);
  });

  test('setting |expand-title| and |collapse-title|', async () => {
    assertFalse(button.expanded);
    button.expandTitle = expandTitle;
    await button.updateComplete;
    assertEquals(expandTitle, button.title);

    button.click();
    button.collapseTitle = collapseTitle;
    await button.updateComplete;
    assertEquals(collapseTitle, button.title);
  });

  test('no tooltip', async () => {
    assertEquals(undefined, button.expandTitle);
    assertEquals(undefined, button.collapseTitle);

    await button.updateComplete;
    assertFalse(button.expanded);
    assertEquals('', button.title);

    button.click();
    await button.updateComplete;
    assertTrue(button.expanded);
    assertEquals('', button.title);
  });

});
