// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('KeyboardAndTextInputPageTests', function() {
  let page = null;

  async function initPage() {
    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);
    await CrSettingsPrefs.initialized;

    page = document.createElement('settings-keyboard-and-text-input-page');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(function() {
    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.A11Y_KEYBOARD_AND_TEXT_INPUT);
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  test('Dictation labels', async () => {
    // Ensure that the Dictation locale menu is shown by setting the dictation
    // pref to true (done in default prefs) and populating dictation locale
    // options with mock data.
    await initPage();

    page.setPrefValue('settings.a11y.dictation', true);
    page.setPrefValue('settings.a11y.dictation_locale', 'en-US');

    const locales = [{
      name: 'English (United States)',
      worksOffline: true,
      installed: true,
      recommended: true,
      value: 'en-US',
    }];
    webUIListenerCallback('dictation-locales-set', locales);
    flush();

    // Dictation toggle.
    const dictationSetting = page.shadowRoot.querySelector('#enableDictation');
    assertTrue(!!dictationSetting);
    assertTrue(dictationSetting.checked);
    assertEquals('Dictation', dictationSetting.label);
    assertEquals(
        'Type with your voice. Use Search + D, then start speaking.',
        dictationSetting.subLabel);

    // Dictation locale menu.
    const dictationLocaleMenuLabel =
        page.shadowRoot.querySelector('#dictationLocaleMenuLabel');
    const dictationLocaleMenuSubtitle =
        page.shadowRoot.querySelector('#dictationLocaleMenuSubtitle');
    assertTrue(!!dictationLocaleMenuLabel);
    assertTrue(!!dictationLocaleMenuSubtitle);
    assertEquals('Language', dictationLocaleMenuLabel.innerText);
    assertEquals(
        'English (United States) is processed locally and works offline',
        dictationLocaleMenuSubtitle.innerText);

    // Fake a request to change the dictation locale menu subtitle.
    webUIListenerCallback('dictation-locale-menu-subtitle-changed', 'Testing');
    flush();

    // Only the dictation locale subtitle should have changed.
    assertEquals('Dictation', dictationSetting.label);
    assertEquals(
        'Type with your voice. Use Search + D, then start speaking.',
        dictationSetting.subLabel);
    assertEquals('Language', dictationLocaleMenuLabel.innerText);
    assertEquals('Testing', dictationLocaleMenuSubtitle.innerText);
  });

  test('Test computeDictationLocaleSubtitle_()', async () => {
    await initPage();
    const locales = [
      {
        name: 'English (United States)',
        worksOffline: true,
        installed: true,
        recommended: true,
        value: 'en-US',
      },
      {
        name: 'Spanish',
        worksOffline: true,
        installed: false,
        recommended: false,
        value: 'es',
      },
      {
        name: 'German',
        // Note: this data should never occur in practice. If a locale isn't
        // supported offline, then it should never be installed. Test this case
        // to verify our code still works given unexpected input.
        worksOffline: false,
        installed: true,
        recommended: false,
        value: 'de',
      },
      {
        name: 'French (France)',
        worksOffline: false,
        installed: false,
        recommended: false,
        value: 'fr-FR',
      },
    ];
    webUIListenerCallback('dictation-locales-set', locales);
    page.dictationLocaleSubtitleOverride_ = 'Testing';
    flush();

    page.setPrefValue('settings.a11y.dictation_locale', 'en-US');
    flush();
    assertEquals(
        'English (United States) is processed locally and works offline',
        page.computeDictationLocaleSubtitle_());

    // Changing the Dictation locale pref should change the subtitle
    // computation.
    page.setPrefValue('settings.a11y.dictation_locale', 'es');
    assertEquals(
        'Couldn’t download Spanish speech files. Download will be attempted ' +
            'later. Speech is sent to Google for processing until download ' +
            'is completed.',
        page.computeDictationLocaleSubtitle_());

    page.setPrefValue('settings.a11y.dictation_locale', 'de');
    assertEquals(
        'German speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());

    page.setPrefValue('settings.a11y.dictation_locale', 'fr-FR');
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());

    // Only use the subtitle override once.
    page.useDictationLocaleSubtitleOverride_ = true;
    assertEquals('Testing', page.computeDictationLocaleSubtitle_());
    assertFalse(page.useDictationLocaleSubtitleOverride_);
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());
  });

  test('some parts are hidden in kiosk mode', async function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();
    flush();

    const subpageLinks = page.root.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => assertFalse(isVisible(subpageLink)));
  });

  test('Deep link to switch access', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
    });
    await initPage();

    const params = new URLSearchParams();
    params.append('settingId', '1522');
    Router.getInstance().navigateTo(
        routes.A11Y_KEYBOARD_AND_TEXT_INPUT, params);

    flush();

    const deepLinkElement = page.shadowRoot.querySelector('#enableSwitchAccess')
                                .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Switch access toggle should be focused for settingId=1522.');
  });

  const selectorRouteList = [
    {selector: '#keyboardSubpageButton', route: routes.KEYBOARD},
  ];

  selectorRouteList.forEach(({selector, route}) => {
    test(
        `should focus ${selector} button when returning from ${
            route.path} subpage`,
        async () => {
          await initPage();
          flush();
          const router = Router.getInstance();

          const subpageButton = page.shadowRoot.querySelector(selector);
          assertTrue(!!subpageButton);

          subpageButton.click();
          assertEquals(route, router.currentRoute);
          assertNotEquals(
              subpageButton, page.shadowRoot.activeElement,
              `${selector} should not be focused`);

          const popStateEventPromise = eventToPromise('popstate', window);
          router.navigateToPreviousRoute();
          await popStateEventPromise;
          await waitBeforeNextRender(page);

          assertEquals(
              routes.A11Y_KEYBOARD_AND_TEXT_INPUT, router.currentRoute);
          assertEquals(
              subpageButton, page.shadowRoot.activeElement,
              `${selector} should be focused`);
        });
  });

  const settingsToggleButtons = [
    {
      id: 'stickyKeysToggle',
      prefKey: 'settings.a11y.sticky_keys_enabled',
    },
    {
      id: 'focusHighlightToggle',
      prefKey: 'settings.a11y.focus_highlight',
    },
    {
      id: 'caretHighlightToggle',
      prefKey: 'settings.a11y.caret_highlight',
    },
    {
      id: 'caretBrowsingToggle',
      prefKey: 'settings.a11y.caretbrowsing.enabled',
    },
  ];

  settingsToggleButtons.forEach(({id, prefKey}) => {
    test(`Accessibility toggle button syncs to prefs: ${id}`, async function() {
      await initPage();
      // Find the toggle and ensure that it's:
      // 1. Not checked
      // 2. The associated pref is off
      const toggle = page.shadowRoot.querySelector(`#${id}`);
      assertTrue(!!toggle);
      assertFalse(toggle.checked);
      let pref = page.getPref(prefKey);
      assertFalse(pref.value);

      // Click the toggle. Ensure that it's:
      // 1. Checked
      // 2. The associated pref is on
      toggle.click();
      assertTrue(toggle.checked);
      pref = page.getPref(prefKey);
      assertTrue(pref.value);
    });
  });
});
