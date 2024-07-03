// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

// clang-format off
// <if expr="is_win or is_linux or is_macosx">
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsPdfOcrToggleElement} from 'chrome://settings/lazy_load.js';
import {ScreenAiInstallStatus} from 'chrome://settings/lazy_load.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// </if>
// clang-format on

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AccessibilityBrowserProxy, LanguageHelper, SettingsA11yPageElement} from 'chrome://settings/lazy_load.js';
import {AccessibilityBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';


class TestAccessibilityBrowserProxy extends TestBrowserProxy implements
    AccessibilityBrowserProxy {
  private pdfOcrState_: ScreenAiInstallStatus;

  constructor() {
    super([
      'openTrackpadGesturesSettings',
      'recordOverscrollHistoryNavigationChanged',
      // <if expr="is_win or is_linux or is_macosx">
      'getScreenAiInstallState',
      // </if>
      'getScreenReaderState',
    ]);

    this.pdfOcrState_ = ScreenAiInstallStatus.NOT_DOWNLOADED;
  }

  openTrackpadGesturesSettings() {
    this.methodCalled('openTrackpadGesturesSettings');
  }

  recordOverscrollHistoryNavigationChanged(enabled: boolean) {
    this.methodCalled('recordOverscrollHistoryNavigationChanged', enabled);
  }

  // <if expr="is_win or is_linux or is_macosx">
  getScreenAiInstallState() {
    this.methodCalled('getScreenAiInstallState');
    return Promise.resolve(this.pdfOcrState_);
  }
  // </if>

  getScreenReaderState() {
    this.methodCalled('getScreenReaderState');
    return Promise.resolve(false);
  }
}

suite('A11yPage', () => {
  let a11yPage: SettingsA11yPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let browserProxy: TestAccessibilityBrowserProxy;
  let languageHelper: LanguageHelper;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      pdfOcrEnabled: true,
    });
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);

    return CrSettingsPrefs.initialized.then(function() {
      // Set up test browser proxy.
      browserProxy = new TestAccessibilityBrowserProxy();
      AccessibilityBrowserProxyImpl.setInstance(browserProxy);

      // Set up languages helper.
      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      a11yPage = document.createElement('settings-a11y-page');
      a11yPage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, a11yPage, 'prefs');

      a11yPage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, a11yPage, 'language-helper');

      document.body.appendChild(a11yPage);
      flush();

      languageHelper = a11yPage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  // <if expr="is_win or is_linux or is_macosx">
  test('check pdf ocr toggle visibility', async () => {
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));

    // Simulate disabling a screen reader to exclude the PDF OCR toggle in a
    // DOM.
    webUIListenerCallback('screen-reader-state-changed', false);

    await flushTasks();
    let pdfOcrToggle =
        a11yPage.shadowRoot!.querySelector<SettingsPdfOcrToggleElement>(
            '#pdfOcrToggle');
    assertFalse(!!pdfOcrToggle);

    // Simulate enabling a screen reader to include the PDF OCR toggle in a
    // DOM.
    webUIListenerCallback('screen-reader-state-changed', true);

    await flushTasks();
    pdfOcrToggle =
        a11yPage.shadowRoot!.querySelector<SettingsPdfOcrToggleElement>(
            '#pdfOcrToggle');
    assertTrue(!!pdfOcrToggle);
    assertTrue(isVisible(pdfOcrToggle));
  });
  // </if>

  // TODO(crbug.com/1499996): Add more test cases to improve code coverage.
});
