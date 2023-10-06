// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsCustomizePenButtonsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTablets, GraphicsTablet, Router, routes} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-customize-pen-buttons-subpage>', () => {
  let page: SettingsCustomizePenButtonsSubpageElement;

  setup(async () => {
    page = document.createElement('settings-customize-pen-buttons-subpage');
    page.graphicsTablets = fakeGraphicsTablets;
    // Set the current route with mouseId as search param and notify
    // the observer to update mouse settings.
    const url = new URLSearchParams(
        {'graphicsTabletId': encodeURIComponent(fakeGraphicsTablets[0]!.id)});
    await Router.getInstance().setCurrentRoute(
        routes.CUSTOMIZE_TABLET_BUTTONS,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('navigate to device page when graphics tablet detached', async () => {
    assertEquals(
        Router.getInstance().currentRoute, routes.CUSTOMIZE_TABLET_BUTTONS);
    const graphicsTablet: GraphicsTablet = page.selectedTablet;
    assertTrue(!!graphicsTablet);
    assertEquals(graphicsTablet.id, fakeGraphicsTablets[0]!.id);
    // Remove fakeMice[0] from the mouse list.
    page.graphicsTablets = [fakeGraphicsTablets[1]!];
    await flushTasks();
    assertEquals(Router.getInstance().currentRoute, routes.DEVICE);
  });
});
