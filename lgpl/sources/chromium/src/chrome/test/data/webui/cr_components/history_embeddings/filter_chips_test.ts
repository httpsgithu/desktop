// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/filter_chips.js';

import type {HistoryEmbeddingsFilterChips, Suggestion} from 'chrome://resources/cr_components/history_embeddings/filter_chips.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('cr-history-embeddings-filter-chips', () => {
  let element: HistoryEmbeddingsFilterChips;

  setup(() => {
    loadTimeData.overrideValues({
      historyEmbeddingsSuggestion1: 'yesterday',
      historyEmbeddingsSuggestion2: 'last 7 days',
      historyEmbeddingsSuggestion3: 'last 30 days',
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    element = document.createElement('cr-history-embeddings-filter-chips');
    document.body.appendChild(element);
    return flushTasks();
  });

  test('UpdatesByGroupChipByBinding', () => {
    assertFalse(element.$.byGroupChip.hasAttribute('selected'));
    assertEquals('history-embeddings:by-group', element.$.byGroupChipIcon.icon);
    element.showResultsByGroup = true;
    assertTrue(element.$.byGroupChip.hasAttribute('selected'));
    assertEquals('cr:check', element.$.byGroupChipIcon.icon);
  });

  test('UpdatesByGroupChipByClicking', async () => {
    let notifyEventPromise =
        eventToPromise('show-results-by-group-changed', element);
    element.$.byGroupChip.click();
    let notifyEvent = await notifyEventPromise;
    assertTrue(element.showResultsByGroup);
    assertTrue(element.$.byGroupChip.hasAttribute('selected'));
    assertTrue(notifyEvent.detail.value);

    notifyEventPromise =
        eventToPromise('show-results-by-group-changed', element);
    element.$.byGroupChip.click();
    notifyEvent = await notifyEventPromise;
    assertFalse(element.showResultsByGroup);
    assertFalse(element.$.byGroupChip.hasAttribute('selected'));
    assertFalse(notifyEvent.detail.value);
  });

  test('SelectingSuggestionsDispatchesEvents', async () => {
    async function clickChipAndGetSelectedSuggestion(chip: HTMLElement):
        Promise<Suggestion> {
      const eventPromise =
          eventToPromise('selected-suggestion-changed', element);
      chip.click();
      const event = await eventPromise;
      return event.detail.value;
    }

    function assertDaysFromToday(days: number, date: Date) {
      const today = new Date();
      today.setHours(0, 0, 0, 0);
      const dateDiff = today.getTime() - date.getTime();
      assertEquals(days, Math.round(dateDiff / 1000 / 60 / 60 / 24));
    }

    const suggestions = element.shadowRoot!.querySelectorAll<HTMLElement>(
        '#suggestions cr-chip');
    const yesterday = await clickChipAndGetSelectedSuggestion(suggestions[0]!);
    assertEquals('yesterday', yesterday.label);
    assertDaysFromToday(1, yesterday.timeRangeStart);
    const last7Days = await clickChipAndGetSelectedSuggestion(suggestions[1]!);
    assertEquals('last 7 days', last7Days.label);
    assertDaysFromToday(7, last7Days.timeRangeStart);
    const last30Days = await clickChipAndGetSelectedSuggestion(suggestions[2]!);
    assertEquals('last 30 days', last30Days.label);
    assertDaysFromToday(30, last30Days.timeRangeStart);
  });

  test('UnselectingSuggestionsDispatchesEvent', async () => {
    const firstChip =
        element.shadowRoot!.querySelector<HTMLElement>('#suggestions cr-chip')!;
    const selectPromise =
        eventToPromise('selected-suggestion-changed', element);
    firstChip.click();
    const selectEvent = await selectPromise;
    assertEquals('yesterday', selectEvent.detail.value.label);

    const unselectPromise =
        eventToPromise('selected-suggestion-changed', element);
    firstChip.click();
    const unselectEvent = await unselectPromise;
    assertEquals(undefined, unselectEvent.detail.value);
  });
});
