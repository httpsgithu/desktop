// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/thread_safe_script_container.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using ScriptStatus = ThreadSafeScriptContainer::ScriptStatus;

const char kKeyUrl[] = "https://example.com/key";

class ThreadSafeScriptContainerTest : public ::testing::Test {
 public:
  ThreadSafeScriptContainerTest()
      : writer_thread_(Platform::Current()->CreateThread(
            ThreadCreationParams(WebThreadType::kTestThread)
                .SetThreadNameForTest("writer_thread"))),
        reader_thread_(Platform::Current()->CreateThread(
            ThreadCreationParams(WebThreadType::kTestThread)
                .SetThreadNameForTest("reader_thread"))),
        container_(base::MakeRefCounted<ThreadSafeScriptContainer>()) {}

 protected:
  WaitableEvent* AddOnWriterThread(
      ThreadSafeScriptContainer::RawScriptData** out_data) {
    PostCrossThreadTask(
        *writer_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(
            [](scoped_refptr<ThreadSafeScriptContainer> container,
               ThreadSafeScriptContainer::RawScriptData** out_data,
               WaitableEvent* waiter) {
              auto data = ThreadSafeScriptContainer::RawScriptData::Create(
                  String::FromUTF8("utf-8") /* encoding */,
                  Vector<Vector<char>>() /* script_text */,
                  Vector<Vector<char>>() /* meta_data */);
              *out_data = data.get();
              container->AddOnIOThread(KURL(kKeyUrl), std::move(data));
              waiter->Signal();
            },
            container_, CrossThreadUnretained(out_data),
            CrossThreadUnretained(&writer_waiter_)));
    return &writer_waiter_;
  }

  WaitableEvent* OnAllDataAddedOnWriterThread() {
    PostCrossThreadTask(
        *writer_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(
            [](scoped_refptr<ThreadSafeScriptContainer> container,
               WaitableEvent* waiter) {
              container->OnAllDataAddedOnIOThread();
              waiter->Signal();
            },
            container_, CrossThreadUnretained(&writer_waiter_)));
    return &writer_waiter_;
  }

  WaitableEvent* GetStatusOnReaderThread(ScriptStatus* out_status) {
    PostCrossThreadTask(
        *reader_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(
            [](scoped_refptr<ThreadSafeScriptContainer> container,
               ScriptStatus* out_status, WaitableEvent* waiter) {
              *out_status = container->GetStatusOnWorkerThread(KURL(kKeyUrl));
              waiter->Signal();
            },
            container_, CrossThreadUnretained(out_status),
            CrossThreadUnretained(&reader_waiter_)));
    return &reader_waiter_;
  }

  WaitableEvent* WaitOnReaderThread(bool* out_exists) {
    PostCrossThreadTask(
        *reader_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(
            [](scoped_refptr<ThreadSafeScriptContainer> container,
               bool* out_exists, WaitableEvent* waiter) {
              *out_exists = container->WaitOnWorkerThread(KURL(kKeyUrl));
              waiter->Signal();
            },
            container_, CrossThreadUnretained(out_exists),
            CrossThreadUnretained(&reader_waiter_)));
    return &reader_waiter_;
  }

  WaitableEvent* TakeOnReaderThread(
      ThreadSafeScriptContainer::RawScriptData** out_data) {
    PostCrossThreadTask(
        *reader_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(
            [](scoped_refptr<ThreadSafeScriptContainer> container,
               ThreadSafeScriptContainer::RawScriptData** out_data,
               WaitableEvent* waiter) {
              auto data = container->TakeOnWorkerThread(KURL(kKeyUrl));
              *out_data = data.get();
              waiter->Signal();
            },
            container_, CrossThreadUnretained(out_data),
            CrossThreadUnretained(&reader_waiter_)));
    return &reader_waiter_;
  }

 private:
  std::unique_ptr<Thread> writer_thread_;
  std::unique_ptr<Thread> reader_thread_;

  WaitableEvent writer_waiter_;
  WaitableEvent reader_waiter_;

  scoped_refptr<ThreadSafeScriptContainer> container_;
};

TEST_F(ThreadSafeScriptContainerTest, WaitExistingKey) {
  {
    ScriptStatus result = ScriptStatus::kReceived;
    GetStatusOnReaderThread(&result)->Wait();
    EXPECT_EQ(ScriptStatus::kPending, result);
  }

  ThreadSafeScriptContainer::RawScriptData* added_data;
  {
    bool result = false;
    WaitableEvent* pending_wait = WaitOnReaderThread(&result);
    // This should not be signaled until data is added.
    EXPECT_FALSE(pending_wait->IsSignaled());
    WaitableEvent* pending_write = AddOnWriterThread(&added_data);
    pending_wait->Wait();
    pending_write->Wait();
    EXPECT_TRUE(result);
  }

  {
    ScriptStatus result = ScriptStatus::kFailed;
    GetStatusOnReaderThread(&result)->Wait();
    EXPECT_EQ(ScriptStatus::kReceived, result);
  }

  {
    ThreadSafeScriptContainer::RawScriptData* taken_data;
    TakeOnReaderThread(&taken_data)->Wait();
    EXPECT_EQ(added_data, taken_data);
  }

  {
    ScriptStatus result = ScriptStatus::kFailed;
    GetStatusOnReaderThread(&result)->Wait();
    // The record should exist though it's already taken.
    EXPECT_EQ(ScriptStatus::kTaken, result);
  }

  {
    bool result = false;
    WaitOnReaderThread(&result)->Wait();
    // Waiting for the record being already taken should succeed.
    EXPECT_TRUE(result);

    // The record status should still be |kTaken|.
    ScriptStatus status = ScriptStatus::kFailed;
    GetStatusOnReaderThread(&status)->Wait();
    EXPECT_EQ(ScriptStatus::kTaken, status);
  }

  // Finish adding data.
  OnAllDataAddedOnWriterThread()->Wait();

  {
    bool result = false;
    WaitOnReaderThread(&result)->Wait();
    // The record is in |kTaken| status, so Wait shouldn't fail.
    EXPECT_TRUE(result);

    // The status of record should still be |kTaken|.
    ScriptStatus status = ScriptStatus::kFailed;
    GetStatusOnReaderThread(&status)->Wait();
    EXPECT_EQ(ScriptStatus::kTaken, status);
  }
}

TEST_F(ThreadSafeScriptContainerTest, WaitNonExistingKey) {
  {
    ScriptStatus result = ScriptStatus::kReceived;
    GetStatusOnReaderThread(&result)->Wait();
    EXPECT_EQ(ScriptStatus::kPending, result);
  }

  {
    bool result = true;
    WaitableEvent* pending_wait = WaitOnReaderThread(&result);
    // This should not be signaled until OnAllDataAdded is called.
    EXPECT_FALSE(pending_wait->IsSignaled());
    WaitableEvent* pending_on_all_data_added = OnAllDataAddedOnWriterThread();
    pending_wait->Wait();
    pending_on_all_data_added->Wait();
    // Aborted wait should return false.
    EXPECT_FALSE(result);
  }

  {
    bool result = true;
    WaitOnReaderThread(&result)->Wait();
    // Wait fails immediately because OnAllDataAdded is called.
    EXPECT_FALSE(result);
  }
}

}  // namespace blink
