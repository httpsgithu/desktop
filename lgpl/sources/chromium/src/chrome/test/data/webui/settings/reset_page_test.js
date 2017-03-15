// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_reset_page', function() {
  /** @enum {string} */
  var TestNames = {
    PowerwashDialogAction: 'PowerwashDialogAction',
    PowerwashDialogOpenClose: 'PowerwashDialogOpenClose',
    ResetBannerClose: 'ResetBannerClose',
    ResetBannerReset: 'ResetBannerReset',
    ResetProfileDialogAction: 'ResetProfileDialogAction',
    ResetProfileDialogOpenClose: 'ResetProfileDialogOpenClose',
    ResetProfileDialogOriginUnknown: 'ResetProfileDialogOriginUnknown',
    ResetProfileDialogOriginUserClick: 'ResetProfileDialogOriginUserClick',
    ResetProfileDialogOriginTriggeredReset:
        'ResetProfileDialogOriginTriggeredReset',
  };

  /**
   * @constructor
   * @implements {settings.ResetBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestResetBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'performResetProfileSettings',
      'onHideResetProfileDialog',
      'onHideResetProfileBanner',
      'onShowResetProfileDialog',
      'showReportedSettings',
      'getTriggeredResetToolName',
      'onPowerwashDialogShow',
    ]);
  };

  TestResetBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    performResetProfileSettings: function(sendSettings, requestOrigin) {
      this.methodCalled('performResetProfileSettings', requestOrigin);
      return Promise.resolve();
    },

    /** @override */
    onHideResetProfileDialog: function() {
      this.methodCalled('onHideResetProfileDialog');
    },

    /** @override */
    onHideResetProfileBanner: function() {
      this.methodCalled('onHideResetProfileBanner');
    },

    /** @override */
    onShowResetProfileDialog: function() {
      this.methodCalled('onShowResetProfileDialog');
    },

    /** @override */
    showReportedSettings: function() {
      this.methodCalled('showReportedSettings');
    },

    /** @override */
    getTriggeredResetToolName: function() {
      this.methodCalled('getTriggeredResetToolName');
      return Promise.resolve('WonderfulAV');
    },

    /** @override */
    onPowerwashDialogShow: function() {
      this.methodCalled('onPowerwashDialogShow');
    },
  };

  function registerBannerTests() {
    suite('BannerTests', function() {
      var resetBanner = null;
      var browserProxy = null;

      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://md-settings/reset_page/reset_profile_banner.html');
      });

      setup(function() {
        browserProxy = new TestResetBrowserProxy();
        settings.ResetBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        resetBanner = document.createElement('settings-reset-profile-banner');
        document.body.appendChild(resetBanner);
      });

      teardown(function() { resetBanner.remove(); });

      // Tests that the reset profile banner
      //  - opens the reset profile dialog when the reset button is clicked.
      //  - reset happens when clicking on the dialog's reset button.
      //  - the reset profile dialog is closed after reset is done.
      test(TestNames.ResetBannerReset, function() {
        var dialog = resetBanner.$$('settings-reset-profile-dialog');
        assertFalse(!!dialog);
        MockInteractions.tap(resetBanner.$.reset);
        Polymer.dom.flush();
        assertTrue(resetBanner.showResetProfileDialog_)
        dialog = resetBanner.$$('settings-reset-profile-dialog');
        assertTrue(!!dialog);

        MockInteractions.tap(dialog.$.reset);

        return browserProxy.whenCalled('performResetProfileSettings')
            .then(PolymerTest.flushTasks)
            .then(function() {
              assertFalse(resetBanner.showResetProfileDialog_);
              dialog = resetBanner.$$('settings-reset-profile-dialog');
              assertFalse(!!dialog);
            });
      });

      // Tests that the reset profile banner removes itself from the DOM when
      // the close button is clicked and that |onHideResetProfileBanner| is
      // called.
      test(TestNames.ResetBannerClose, function() {
        MockInteractions.tap(resetBanner.$.close);
        assertFalse(!!resetBanner.parentNode);
        return browserProxy.whenCalled('onHideResetProfileBanner');
      });
    });
  }

  function registerDialogTests() {
    suite('DialogTests', function() {
      var resetPage = null;

      /** @type {!settings.ResetPageBrowserProxy} */
      var resetPageBrowserProxy = null;

      /** @type {!settings.LifetimeBrowserProxy} */
      var lifetimeBrowserProxy = null;

      setup(function() {
        if (cr.isChromeOS) {
          lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
          settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;
        }

        resetPageBrowserProxy = new TestResetBrowserProxy();
        settings.ResetBrowserProxyImpl.instance_ = resetPageBrowserProxy;

        PolymerTest.clearBody();
        resetPage = document.createElement('settings-reset-page');
        document.body.appendChild(resetPage);
      });

      teardown(function() { resetPage.remove(); });

      /**
       * @param {function(SettingsResetProfileDialogElemeent)}
       *     closeDialogFn A function to call for closing the dialog.
       * @return {!Promise}
       */
      function testOpenCloseResetProfileDialog(closeDialogFn) {
        resetPageBrowserProxy.resetResolver('onShowResetProfileDialog');
        resetPageBrowserProxy.resetResolver('onHideResetProfileDialog');

        // Open reset profile dialog.
        MockInteractions.tap(resetPage.$.resetProfile);
        Polymer.dom.flush();
        var dialog = resetPage.$$('settings-reset-profile-dialog');
        assertTrue(!!dialog);
        var onDialogClosed = new Promise(
            function(resolve, reject) {
              dialog.addEventListener('close', function() {
                assertFalse(dialog.$.dialog.open);
                resolve();
              });
            });

        return PolymerTest.flushTasks().then(function() {
          resetPageBrowserProxy.whenCalled('onShowResetProfileDialog')
              .then(function() {
                assertTrue(dialog.$.dialog.open);
                closeDialogFn(dialog);
                return Promise.all([
                  onDialogClosed,
                  resetPageBrowserProxy.whenCalled('onHideResetProfileDialog'),
                ]);
              });
        });
      }

      // Tests that the reset profile dialog opens and closes correctly and that
      // resetPageBrowserProxy calls are occurring as expected.
      test(TestNames.ResetProfileDialogOpenClose, function() {
        return testOpenCloseResetProfileDialog(function(dialog) {
          // Test case where the 'cancel' button is clicked.
          MockInteractions.tap(dialog.$.cancel);
        }).then(PolymerTest.flushTasks).then(function() {
          return testOpenCloseResetProfileDialog(function(dialog) {
            // Test case where the 'close' button is clicked.
            MockInteractions.tap(dialog.$.dialog.getCloseButton());
          });
        });
      });

      // Tests that when user request to reset the profile the appropriate
      // message is sent to the browser.
      test(TestNames.ResetProfileDialogAction, function() {
        // Open reset profile dialog.
        MockInteractions.tap(resetPage.$.resetProfile);
        Polymer.dom.flush();
        var dialog = resetPage.$$('settings-reset-profile-dialog');
        assertTrue(!!dialog);

        var showReportedSettingsLink = dialog.$$('.footer a');
        assertTrue(!!showReportedSettingsLink);
        MockInteractions.tap(showReportedSettingsLink);

        return resetPageBrowserProxy.whenCalled('showReportedSettings').then(
            function() {
              assertFalse(dialog.$.reset.disabled);
              assertFalse(dialog.$.resetSpinner.active);
              MockInteractions.tap(dialog.$.reset);
              assertTrue(dialog.$.reset.disabled);
              assertTrue(dialog.$.cancel.disabled);
              assertTrue(dialog.$.resetSpinner.active);
              return resetPageBrowserProxy.whenCalled(
                  'performResetProfileSettings');
            });
      });

      function testResetRequestOrigin(expectedOrigin) {
        var dialog = resetPage.$$('settings-reset-profile-dialog');
        assertTrue(!!dialog);
        MockInteractions.tap(dialog.$.reset);
        return resetPageBrowserProxy.whenCalled(
            'performResetProfileSettings').then(function(resetRequest) {
              assertEquals(expectedOrigin, resetRequest);
            });
      }

      test(TestNames.ResetProfileDialogOriginUnknown, function() {
        settings.navigateTo(settings.Route.RESET_DIALOG);
        return resetPageBrowserProxy.whenCalled('onShowResetProfileDialog')
            .then(function() { return testResetRequestOrigin(''); });
      });

      test(TestNames.ResetProfileDialogOriginUserClick, function() {
        MockInteractions.tap(resetPage.$.resetProfile);
        return resetPageBrowserProxy.whenCalled('onShowResetProfileDialog')
            .then(function() { return testResetRequestOrigin('userclick'); });
      });

      test(TestNames.ResetProfileDialogOriginTriggeredReset, function() {
        settings.navigateTo(settings.Route.TRIGGERED_RESET_DIALOG);
        return resetPageBrowserProxy.whenCalled('onShowResetProfileDialog')
            .then(function() {
              return testResetRequestOrigin('triggeredreset');
            });
      });

      if (cr.isChromeOS) {
        /**
         * @param {function(SettingsPowerwashDialogElemeent):!Element}
         *     closeButtonFn A function that returns the button to be used for
         *     closing the dialog.
         * @return {!Promise}
         */
        function testOpenClosePowerwashDialog(closeButtonFn) {
          // Open powerwash dialog.
          MockInteractions.tap(resetPage.$.powerwash);
          Polymer.dom.flush();
          var dialog = resetPage.$$('settings-powerwash-dialog');
          assertTrue(!!dialog);
          assertTrue(dialog.$.dialog.open);
          var onDialogClosed = new Promise(
            function(resolve, reject) {
              dialog.addEventListener('close', function() {
                assertFalse(dialog.$.dialog.open);
                resolve();
              });
            });

          MockInteractions.tap(closeButtonFn(dialog));
          return Promise.all([
              onDialogClosed,
              resetPageBrowserProxy.whenCalled('onPowerwashDialogShow'),
          ]);
        }

        // Tests that the powerwash dialog opens and closes correctly, and
        // that chrome.send calls are propagated as expected.
        test(TestNames.PowerwashDialogOpenClose, function() {
          // Test case where the 'cancel' button is clicked.
          return testOpenClosePowerwashDialog(function(dialog) {
            return dialog.$.cancel;
          }).then(function() {
            // Test case where the 'close' button is clicked.
            return testOpenClosePowerwashDialog(function(dialog) {
              return dialog.$.dialog.getCloseButton();
            });
          });
        });

        // Tests that when powerwash is requested chrome.send calls are
        // propagated as expected.
        test(TestNames.PowerwashDialogAction, function() {
          // Open powerwash dialog.
          MockInteractions.tap(resetPage.$.powerwash);
          Polymer.dom.flush();
          var dialog = resetPage.$$('settings-powerwash-dialog');
          assertTrue(!!dialog);
          MockInteractions.tap(dialog.$.powerwash);
          return lifetimeBrowserProxy.whenCalled('factoryReset');
        });
      }
    });
  }

  return {
    registerTests: function() {
      registerBannerTests();
      registerDialogTests();
    },
  };
});
