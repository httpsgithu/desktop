// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-detail-view. */
cr.define('extension_error_page_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Layout: 'layout',
    CodeSection: 'code section',
  };

  /**
   * @constructor
   * @extends {extension_test_util.ClickMock}
   * @implements {extensions.ErrorPageDelegate}
   */
  function MockErrorPageDelegate() {}

  MockErrorPageDelegate.prototype = {
    __proto__: extension_test_util.ClickMock.prototype,

    /** @override */
    deleteErrors: function(extensionId, errorIds, type) {},

    /** @override */
    requestFileSource: function(args) {
      this.requestFileSourceArgs = args;
      this.requestFileSourceResolver = new PromiseResolver();
      return this.requestFileSourceResolver.promise;
    },
  };

  function registerTests() {
    suite('ExtensionErrorPageTest', function() {
      /** @type {chrome.developerPrivate.ExtensionInfo} */
      var extensionData;

      /** @type {extensions.ErrorPage} */
      var errorPage;

      /** @type {MockErrorPageDelegate} */
      var mockDelegate;

      var extensionId = 'a'.repeat(32);

      // Common data for runtime errors.
      var runtimeErrorBase = {
        type: chrome.developerPrivate.ErrorType.RUNTIME,
        extensionId: extensionId,
        fromIncognito: false,
      };

      // Common data for manifest errors.
      var manifestErrorBase = {
        type: chrome.developerPrivate.ErrorType.MANIFEST,
        extensionId: extensionId,
        fromIncognito: false,
      };

      suiteSetup(function() {
        return PolymerTest.importHtml('chrome://extensions/error_page.html');
      });

      // Initialize an extension item before each test.
      setup(function() {
        PolymerTest.clearBody();
        var runtimeError = Object.assign({
          source: 'chrome-extension://' + extensionId + '/source.html',
          message: 'message',
          id: 1,
          severity: chrome.developerPrivate.ErrorLevel.ERROR,
        }, runtimeErrorBase);
        extensionData = extension_test_util.createExtensionInfo({
          runtimeErrors: [runtimeError],
          manifestErrors: [],
        });
        errorPage = new extensions.ErrorPage();
        mockDelegate = new MockErrorPageDelegate();
        errorPage.delegate = mockDelegate;
        errorPage.data = extensionData;
        document.body.appendChild(errorPage);
      });

      test(assert(TestNames.Layout), function() {
        Polymer.dom.flush();

        extension_test_util.testIronIcons(errorPage);

        var testIsVisible = extension_test_util.isVisible.bind(null, errorPage);
        expectTrue(testIsVisible('#close-button'));
        expectTrue(testIsVisible('#heading'));
        expectTrue(testIsVisible('#errors-list'));

        var errorElems = errorPage.querySelectorAll('* /deep/ .error-item');
        expectEquals(1, errorElems.length);
        var error = errorElems[0];
        expectEquals(
            'message',
            error.querySelector('.error-message').textContent.trim());
        expectTrue(error.querySelector('img').classList.contains(
            'icon-severity-fatal'));

        var manifestError = Object.assign({
          source: 'manifest.json',
          message: 'invalid key',
          id: 2,
          manifestKey: 'permissions',
        }, manifestErrorBase);
        errorPage.set('data.manifestErrors', [manifestError]);
        Polymer.dom.flush();
        var errorElems = errorPage.querySelectorAll('* /deep/ .error-item');
        expectEquals(2, errorElems.length);
        var error = errorElems[0];
        expectEquals(
            'invalid key',
            error.querySelector('.error-message').textContent.trim());
        expectTrue(error.querySelector('img').classList.contains(
            'icon-severity-warning'));

        mockDelegate.testClickingCalls(
            error.querySelector('.delete-button'), 'deleteErrors',
            [extensionId, [manifestError.id]]);
      });

      test(assert(TestNames.CodeSection), function(done) {
        Polymer.dom.flush();

        expectTrue(!!mockDelegate.requestFileSourceArgs);
        args = mockDelegate.requestFileSourceArgs;
        expectEquals(extensionId, args.extensionId);
        expectEquals('source.html', args.pathSuffix);
        expectEquals('message', args.message);

        expectTrue(!!mockDelegate.requestFileSourceResolver);
        var code = {
          beforeHighlight: 'foo',
          highlight: 'bar',
          afterHighlight: 'baz',
          message: 'quu',
        };
        mockDelegate.requestFileSourceResolver.resolve(code);
        mockDelegate.requestFileSourceResolver.promise.then(function() {
          Polymer.dom.flush();
          expectEquals(code, errorPage.$$('extensions-code-section').code);
          done();
        });
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
