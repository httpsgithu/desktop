// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType, OpenPdfParamsParser} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const URL = 'http://xyz.pdf';

function getParamsParser(): OpenPdfParamsParser {
  const paramsParser = new OpenPdfParamsParser(function(destination: string) {
    // Set the dummy viewport dimensions for calculating the zoom level for
    // view destination with 'FitR' type.
    paramsParser.setViewportDimensions({width: 300, height: 500});

    if (destination === 'RU') {
      return Promise.resolve(
          {messageId: 'getNamedDestination_1', pageNumber: 26});
    }
    if (destination === 'US') {
      return Promise.resolve(
          {messageId: 'getNamedDestination_2', pageNumber: 0});
    }
    if (destination === 'UY') {
      return Promise.resolve(
          {messageId: 'getNamedDestination_3', pageNumber: 22});
    }
    if (destination === 'DestWithXYZ') {
      return Promise.resolve({
        messageId: 'getNamedDestination_4',
        namedDestinationView: 'XYZ,111,222,1.7',
        pageNumber: 10,
      });
    }
    if (destination === 'DestWithXYZAtZoomNull') {
      return Promise.resolve({
        messageId: 'getNamedDestination_5',
        namedDestinationView: 'XYZ,111,222,null',
        pageNumber: 10,
      });
    }
    if (destination === 'DestWithXYZWithX0') {
      return Promise.resolve({
        messageId: 'getNamedDestination_6',
        namedDestinationView: 'XYZ,0,200,1.7',
        pageNumber: 11,
      });
    }
    if (destination === 'DestWithXYZWithXNull') {
      return Promise.resolve({
        messageId: 'getNamedDestination_7',
        namedDestinationView: 'XYZ,null,200,1.7',
        pageNumber: 11,
      });
    }
    if (destination === 'DestWithXYZWithY0') {
      return Promise.resolve({
        messageId: 'getNamedDestination_8',
        namedDestinationView: 'XYZ,100,0,1.7',
        pageNumber: 11,
      });
    }
    if (destination === 'DestWithXYZWithYNull') {
      return Promise.resolve({
        messageId: 'getNamedDestination_9',
        namedDestinationView: 'XYZ,100,null,1.7',
        pageNumber: 11,
      });
    }
    if (destination === 'DestWithFitR') {
      return Promise.resolve({
        messageId: 'getNamedDestination_10',
        namedDestinationView: 'FitR,20,100,120,300',
        pageNumber: 0,
      });
    }
    if (destination === 'DestWithFitRReversedCoordinates') {
      return Promise.resolve({
        messageId: 'getNamedDestination_11',
        namedDestinationView: 'FitR,120,300,20,100',
        pageNumber: 0,
      });
    }
    if (destination === 'DestWithFitRWithNull') {
      return Promise.resolve({
        messageId: 'getNamedDestination_12',
        namedDestinationView: 'FitR,null,100,100,300',
        pageNumber: 0,
      });
    }
    return Promise.resolve(
        {messageId: 'getNamedDestination_13', pageNumber: -1});
  });
  return paramsParser;
}

chrome.test.runTests([
  /**
   * Test various open parameters.
   */
  async function testParamsParser() {
    const paramsParser = getParamsParser();

    // Checking #nameddest.
    let params = await paramsParser.getViewportFromUrlParams(`${URL}#RU`);
    chrome.test.assertEq(26, params.page);

    // Checking #nameddest=name.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#nameddest=US`);
    chrome.test.assertEq(0, params.page);

    // Checking #page=pagenum nameddest.The document first page has a pagenum
    // value of 1.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#page=6`);
    chrome.test.assertEq(5, params.page);

    // Checking #zoom=scale.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#zoom=200`);
    chrome.test.assertEq(2, params.zoom);

    // Checking #zoom=scale,left,top.
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#zoom=200,100,200`);
    chrome.test.assertEq(2, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertEq(200, params.position!.y);

    // Checking #nameddest=name and zoom=scale.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=UY&zoom=150`);
    chrome.test.assertEq(22, params.page);
    chrome.test.assertEq(1.5, params.zoom);

    // Checking #page=pagenum and zoom=scale.
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#page=2&zoom=250`);
    chrome.test.assertEq(1, params.page);
    chrome.test.assertEq(2.5, params.zoom);

    // Checking #nameddest=name and zoom=scale,left,top.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=UY&zoom=150,100,200`);
    chrome.test.assertEq(22, params.page);
    chrome.test.assertEq(1.5, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertEq(200, params.position!.y);

    // Checking #page=pagenum and zoom=scale,left,top.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#page=2&zoom=250,100,200`);
    chrome.test.assertEq(1, params.page);
    chrome.test.assertEq(2.5, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertEq(200, params.position!.y);

    chrome.test.succeed();
  },
  /**
   * Test named destinations that specify view fit types.
   */
  async function testParamsNamedDestWithViewFit() {
    const paramsParser = getParamsParser();

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" with multiple valid parameters.
    let params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZ`);
    chrome.test.assertEq(10, params.page);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertEq(111, params.position!.x);
    chrome.test.assertEq(222, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" with a zoom parameter of null.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZAtZoomNull`);
    chrome.test.assertEq(10, params.page);
    chrome.test.assertEq(undefined, params.zoom);
    chrome.test.assertEq(111, params.position!.x);
    chrome.test.assertEq(222, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and its X parameter is 0.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZWithX0`);
    chrome.test.assertEq(11, params.page);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertEq(0, params.position!.x);
    chrome.test.assertEq(200, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and its X parameter is null.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZWithXNull`);
    chrome.test.assertEq(11, params.page);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertTrue(Number.isNaN(params.position!.x));
    chrome.test.assertEq(200, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and its Y parameter is 0.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZWithY0`);
    chrome.test.assertEq(11, params.page);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertEq(0, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and its Y parameter is null.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZWithYNull`);
    chrome.test.assertEq(11, params.page);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertTrue(Number.isNaN(params.position!.y));
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with multiple valid parameters.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithFitR`);
    chrome.test.assertEq(0, params.page);
    chrome.test.assertEq(2.5, params.zoom);
    chrome.test.assertEq(20, params.position!.x);
    chrome.test.assertEq(100, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with multiple valid parameters.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithFitRReversedCoordinates`);
    chrome.test.assertEq(0, params.page);
    chrome.test.assertEq(2.5, params.zoom);
    chrome.test.assertEq(20, params.position!.x);
    chrome.test.assertEq(100, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with one NULL parameters.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithFitRWithNull`);
    chrome.test.assertEq(0, params.page);
    chrome.test.assertEq(undefined, params.zoom);
    chrome.test.assertEq(undefined, params.position);
    chrome.test.assertEq(undefined, params.viewPosition);

    chrome.test.succeed();
  },
  /**
   * Test view params.
   */
  async function testParamsView() {
    const paramsParser = getParamsParser();

    // Checking #view=Fit.
    let params = await paramsParser.getViewportFromUrlParams(`${URL}#view=Fit`);
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=FitH.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitH`);
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=FitH,[int position].
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitH,789`);
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
    chrome.test.assertEq(789, params.viewPosition);

    // Checking #view=FitH,[float position].
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitH,7.89`);
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
    chrome.test.assertEq(7.89, params.viewPosition);

    // Checking #view=FitV.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitV`);
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=FitV,[int position].
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitV,123`);
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
    chrome.test.assertEq(123, params.viewPosition);

    // Checking #view=FitV,[float position].
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitV,1.23`);
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
    chrome.test.assertEq(1.23, params.viewPosition);

    // Checking #view=[wrong parameter].
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitW`);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter],[position].
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitW,555`);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter].
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=XYZ`);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter],[position].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=XYZ,111,222,1.7`);
    chrome.test.assertEq(undefined, params.zoom);
    chrome.test.assertEq(undefined, params.position);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter].
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitR`);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter],[position].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=FitR,20,100,120,300`);
    chrome.test.assertEq(undefined, params.zoom);
    chrome.test.assertEq(undefined, params.position);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    chrome.test.succeed();
  },
  /**
   * Test toolbar and navpane params.
   */
  function testParamsToolbarAndNavpane() {
    const paramsParser = getParamsParser();

    // Checking #toolbar=0 to disable the toolbar.
    chrome.test.assertFalse(paramsParser.shouldShowToolbar(`${URL}#toolbar=0`));
    chrome.test.assertTrue(paramsParser.shouldShowToolbar(`${URL}#toolbar=1`));

    // Checking #navpanes=0 to collapse the sidenav.
    chrome.test.assertFalse(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=0`, false));
    chrome.test.assertFalse(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=0`, true));
    chrome.test.assertTrue(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=1`, false));
    chrome.test.assertTrue(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=1`, true));

    // Checking #navpanes=0&toolbars=1 shows the toolbar and a collapsed
    // sidenav.
    chrome.test.assertTrue(
        paramsParser.shouldShowToolbar(`${URL}#navpanes=0&toolbar=1`));
    chrome.test.assertFalse(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=0&toolbar=1`, false));
    chrome.test.assertFalse(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=0&toolbar=1`, true));

    // Checking #navpanes=1&toolbars=0 shows the toolbar and the sidenav.
    chrome.test.assertTrue(
        paramsParser.shouldShowToolbar(`${URL}#navpanes=1&toolbar=0`));
    chrome.test.assertTrue(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=1&toolbar=0`, false));
    chrome.test.assertTrue(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=1&toolbar=0`, true));

    // Checking no relevant parameters defaults to !sidenavCollapsed.
    chrome.test.assertFalse(paramsParser.shouldShowSidenav(`${URL}`, true));
    chrome.test.assertTrue(paramsParser.shouldShowSidenav(`${URL}`, false));

    chrome.test.succeed();
  },
]);
