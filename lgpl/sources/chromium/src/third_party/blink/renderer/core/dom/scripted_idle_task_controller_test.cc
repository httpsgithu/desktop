// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_callback.h"
#include "third_party/blink/renderer/core/dom/idle_request_options.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_custom_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {
namespace {

enum class ShouldYield { YIELD, DONT_YIELD };

class MockScriptedIdleTaskControllerScheduler final : public ThreadScheduler {
 public:
  explicit MockScriptedIdleTaskControllerScheduler(ShouldYield should_yield)
      : should_yield_(should_yield == ShouldYield::YIELD) {}
  ~MockScriptedIdleTaskControllerScheduler() override = default;

  // ThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    return nullptr;
  }
  void Shutdown() override {}
  bool ShouldYieldForHighPriorityWork() override { return should_yield_; }
  bool CanExceedIdleDeadlineIfRequired() const override { return false; }
  void PostIdleTask(const base::Location&,
                    Thread::IdleTask idle_task) override {
    idle_task_ = std::move(idle_task);
  }
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override {}
  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override {
    return nullptr;
  }
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override {
    return nullptr;
  }

  base::TimeTicks MonotonicallyIncreasingVirtualTime() override {
    return base::TimeTicks();
  }

  void AddTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override {}

  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override {}

  void AddRAILModeObserver(scheduler::WebRAILModeObserver*) override {}

  scheduler::NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() override {
    return nullptr;
  }

  void RunIdleTask() { std::move(idle_task_).Run(TimeTicks()); }
  bool HasIdleTask() const { return !!idle_task_; }

 private:
  bool should_yield_;
  Thread::IdleTask idle_task_;

  DISALLOW_COPY_AND_ASSIGN(MockScriptedIdleTaskControllerScheduler);
};

class MockIdleTask : public ScriptedIdleTaskController::IdleTask {
 public:
  MOCK_METHOD1(invoke, void(IdleDeadline*));
};
}  // namespace

class ScriptedIdleTaskControllerTest : public testing::Test {
 public:
  void SetUp() override { execution_context_ = new NullExecutionContext(); }

 protected:
  Persistent<ExecutionContext> execution_context_;
};

TEST_F(ScriptedIdleTaskControllerTest, RunCallback) {
  MockScriptedIdleTaskControllerScheduler scheduler(ShouldYield::DONT_YIELD);
  ScopedTestingPlatformSupport<TestingPlatformSupportWithCustomScheduler,
                               ThreadScheduler*>
      platform(&scheduler);

  NullExecutionContext execution_context;
  ScriptedIdleTaskController* controller =
      ScriptedIdleTaskController::Create(execution_context_);

  Persistent<MockIdleTask> idle_task(new MockIdleTask());
  IdleRequestOptions options;
  EXPECT_FALSE(scheduler.HasIdleTask());
  int id = controller->RegisterCallback(idle_task, options);
  EXPECT_TRUE(scheduler.HasIdleTask());
  EXPECT_NE(0, id);

  EXPECT_CALL(*idle_task, invoke(testing::_));
  scheduler.RunIdleTask();
  testing::Mock::VerifyAndClearExpectations(idle_task);
  EXPECT_FALSE(scheduler.HasIdleTask());
}

TEST_F(ScriptedIdleTaskControllerTest, DontRunCallbackWhenAskedToYield) {
  MockScriptedIdleTaskControllerScheduler scheduler(ShouldYield::YIELD);
  ScopedTestingPlatformSupport<TestingPlatformSupportWithCustomScheduler,
                               ThreadScheduler*>
      platform(&scheduler);

  NullExecutionContext execution_context;
  ScriptedIdleTaskController* controller =
      ScriptedIdleTaskController::Create(execution_context_);

  Persistent<MockIdleTask> idle_task(new MockIdleTask());
  IdleRequestOptions options;
  int id = controller->RegisterCallback(idle_task, options);
  EXPECT_NE(0, id);

  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(0);
  scheduler.RunIdleTask();
  testing::Mock::VerifyAndClearExpectations(idle_task);

  // The idle task should have been reposted.
  EXPECT_TRUE(scheduler.HasIdleTask());
}

}  // namespace blink
