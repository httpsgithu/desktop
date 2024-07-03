// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://extended-updates-dialog/extended_updates.mojom-webui.js';
import {ExtendedUpdatesBrowserProxy} from 'chrome://extended-updates-dialog/extended_updates_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestExtendedUpdatesBrowserProxy extends TestBrowserProxy implements
    ExtendedUpdatesBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  private optInSuccess: boolean;

  constructor() {
    super([
      'optInToExtendedUpdates',
      'closeDialog',
    ]);

    this.callbackRouter = new PageCallbackRouter();
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.optInSuccess = true;
  }

  async optInToExtendedUpdates(): Promise<boolean> {
    this.methodCalled('optInToExtendedUpdates');
    return Promise.resolve(this.optInSuccess);
  }

  setOptInSuccess(success: boolean): void {
    this.optInSuccess = success;
  }

  closeDialog(): void {
    this.methodCalled('closeDialog');
  }
}
