/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/timing/memory_info.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

TEST(MemoryInfo, quantizeMemorySize) {
  EXPECT_EQ(10000000u, QuantizeMemorySize(1024));
  EXPECT_EQ(10000000u, QuantizeMemorySize(1024 * 1024));
  EXPECT_EQ(410000000u, QuantizeMemorySize(389472983));
  EXPECT_EQ(39600000u, QuantizeMemorySize(38947298));
  EXPECT_EQ(29400000u, QuantizeMemorySize(28947298));
  EXPECT_EQ(19300000u, QuantizeMemorySize(18947298));
  EXPECT_EQ(14300000u, QuantizeMemorySize(13947298));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3894729));
  EXPECT_EQ(10000000u, QuantizeMemorySize(389472));
  EXPECT_EQ(10000000u, QuantizeMemorySize(38947));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3894));
  EXPECT_EQ(10000000u, QuantizeMemorySize(389));
  EXPECT_EQ(10000000u, QuantizeMemorySize(38));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3));
  EXPECT_EQ(10000000u, QuantizeMemorySize(1));
  EXPECT_EQ(10000000u, QuantizeMemorySize(0));
}

static constexpr int kModForBucketizationCheck = 100000;

// The current time per the MemoryInfo Tests.
// Use a large value as a start so that when subtracting twenty minutes it does
// not become negative.
static double current_time_ = 60 * 60;

class MemoryInfoTest : public testing::Test {
 protected:
  void SetUp() override {
    // Use a large value so that when subtracting twenty minutes it does not
    // become negative. current_time_ = 60 * 60;
    original_time_function_ = SetTimeFunctionsForTesting(MockTimeFunction);
    // Advance clock by a large amount so that if there were previous MemoryInfo
    // values, then they are no longer cached.
    AdvanceClock(300 * 60);
  }

  void TearDown() override {
    SetTimeFunctionsForTesting(original_time_function_);
  }

  void AdvanceClock(double seconds) { current_time_ += seconds; }

  void CheckValues(MemoryInfo* info, MemoryInfo::Precision precision) {
    // Check that used <= total <= limit.

    // TODO(npm): add a check usedJSHeapSize <= totalJSHeapSize once it always
    // holds. See https://crbug.com/849322
    EXPECT_LE(info->totalJSHeapSize(), info->jsHeapSizeLimit());
    if (precision == MemoryInfo::Precision::Bucketized) {
      // Check that the bucketized values are heavily rounded.
      EXPECT_EQ(0u, info->totalJSHeapSize() % kModForBucketizationCheck);
      EXPECT_EQ(0u, info->usedJSHeapSize() % kModForBucketizationCheck);
      EXPECT_EQ(0u, info->jsHeapSizeLimit() % kModForBucketizationCheck);
    } else {
      // Check that the precise values are not heavily rounded.
      // Note: these checks are potentially flaky but in practice probably never
      // flaky. If this is noticed to be flaky, disable test and assign bug to
      // npm@.
      EXPECT_NE(0u, info->totalJSHeapSize() % kModForBucketizationCheck);
      EXPECT_NE(0u, info->usedJSHeapSize() % kModForBucketizationCheck);
      EXPECT_NE(0u, info->jsHeapSizeLimit() % kModForBucketizationCheck);
    }
  }

  void CheckEqual(MemoryInfo* info, MemoryInfo* info2) {
    EXPECT_EQ(info2->totalJSHeapSize(), info->totalJSHeapSize());
    EXPECT_EQ(info2->usedJSHeapSize(), info->usedJSHeapSize());
    EXPECT_EQ(info2->jsHeapSizeLimit(), info->jsHeapSizeLimit());
  }

 private:
  static double MockTimeFunction() { return current_time_; }

  TimeFunction original_time_function_;
};

TEST_F(MemoryInfoTest, Bucketized) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  // The vector is used to keep the objects
  // allocated alive even if GC happens. In practice, the objects only get GC'd
  // after we go out of V8TestingScope. But having them in a vector makes it
  // impossible for GC to clear them up unexpectedly early.
  std::vector<v8::Local<v8::ArrayBuffer>> objects;

  MemoryInfo* bucketized_memory =
      MemoryInfo::Create(MemoryInfo::Precision::Bucketized);

  // Check that the values are monotone and rounded.
  CheckValues(bucketized_memory, MemoryInfo::Precision::Bucketized);

  // Advance the clock for a minute. Not enough to make bucketized value
  // recalculate. Also allocate some memory.
  AdvanceClock(60);
  objects.push_back(v8::ArrayBuffer::New(isolate, 100));

  MemoryInfo* bucketized_memory2 =
      MemoryInfo::Create(MemoryInfo::Precision::Bucketized);
  // The old bucketized values must be equal to the new bucketized values.
  CheckEqual(bucketized_memory, bucketized_memory2);

  // TODO(npm): The bucketized MemoryInfo is very hard to change reliably. One
  // option is to do something such as:
  // for (int i = 0; i < kNumArrayBuffersForLargeAlloc; i++)
  //   objects.push_back(v8::ArrayBuffer::New(isolate, 1));
  // Here, kNumArrayBuffersForLargeAlloc should be strictly greater than 200000
  // (test failed on Windows with this value). Creating a single giant
  // ArrayBuffer does not seem to work, so instead a lot of small ArrayBuffers
  // are used. For now we only test that values are still rounded after adding
  // some memory.
  for (int i = 0; i < 10; i++) {
    // Advance the clock for another thirty minutes, enough to make the
    // bucketized value recalculate.
    AdvanceClock(60 * 30);
    objects.push_back(v8::ArrayBuffer::New(isolate, 100));
    MemoryInfo* bucketized_memory3 =
        MemoryInfo::Create(MemoryInfo::Precision::Bucketized);
    CheckValues(bucketized_memory3, MemoryInfo::Precision::Bucketized);
    // The limit should remain unchanged.
    EXPECT_EQ(bucketized_memory3->jsHeapSizeLimit(),
              bucketized_memory->jsHeapSizeLimit());
  }
}

TEST_F(MemoryInfoTest, Precise) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  std::vector<v8::Local<v8::ArrayBuffer>> objects;

  MemoryInfo* precise_memory =
      MemoryInfo::Create(MemoryInfo::Precision::Precise);
  // Check that the precise values are monotone and not heavily rounded.
  CheckValues(precise_memory, MemoryInfo::Precision::Precise);

  // Advance the clock for a nanosecond, which should not be enough to make the
  // precise value recalculate.
  AdvanceClock(1e-9);
  // Allocate an object in heap and keep it in a vector to make sure that it
  // does not get accidentally GC'd. This single ArrayBuffer should be enough to
  // be noticed by the used heap size in the precise MemoryInfo case.
  objects.push_back(v8::ArrayBuffer::New(isolate, 100));
  MemoryInfo* precise_memory2 =
      MemoryInfo::Create(MemoryInfo::Precision::Precise);
  // The old precise values must be equal to the new precise values.
  CheckEqual(precise_memory, precise_memory2);

  for (int i = 0; i < 10; i++) {
    // Advance the clock for another thirty seconds, enough to make the precise
    // values be recalculated. Also allocate another object.
    AdvanceClock(30);
    objects.push_back(v8::ArrayBuffer::New(isolate, 100));

    MemoryInfo* new_precise_memory =
        MemoryInfo::Create(MemoryInfo::Precision::Precise);

    CheckValues(new_precise_memory, MemoryInfo::Precision::Precise);
    // The old precise used heap size must be different from the new one.
    EXPECT_NE(new_precise_memory->usedJSHeapSize(),
              precise_memory->usedJSHeapSize());
    // The limit should remain unchanged.
    EXPECT_EQ(new_precise_memory->jsHeapSizeLimit(),
              precise_memory->jsHeapSizeLimit());
    // Update |precise_memory| to be the newest MemoryInfo thus far.
    precise_memory = new_precise_memory;
  }
}

TEST_F(MemoryInfoTest, FlagEnabled) {
  ScopedPreciseMemoryInfoForTest precise_memory_info(true);
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  std::vector<v8::Local<v8::ArrayBuffer>> objects;

  // Using MemoryInfo::Precision::Bucketized to ensure that the runtime-enabled
  // flag overrides the Precision passed onto the method.
  MemoryInfo* precise_memory =
      MemoryInfo::Create(MemoryInfo::Precision::Bucketized);
  // Check that the precise values are monotone and not heavily rounded.
  CheckValues(precise_memory, MemoryInfo::Precision::Precise);

  // Allocate an object in heap and keep it in a vector to make sure that it
  // does not get accidentally GC'd. This single ArrayBuffer should be enough to
  // be noticed by the used heap size immediately since the
  // PreciseMemoryInfoEnabled flag is on.
  objects.push_back(v8::ArrayBuffer::New(isolate, 100));
  MemoryInfo* precise_memory2 =
      MemoryInfo::Create(MemoryInfo::Precision::Bucketized);
  CheckValues(precise_memory2, MemoryInfo::Precision::Precise);
  // The old precise JS heap size value must NOT be equal to the new value.
  EXPECT_NE(precise_memory2->usedJSHeapSize(),
            precise_memory->usedJSHeapSize());
}

TEST_F(MemoryInfoTest, ZeroTime) {
  // In this test, we make sure that even if the CurrentTimeTicks() value is
  // very close to 0, we still obtain memory information from the first call to
  // MemoryInfo::Create. We cannot just subtract CurrentTimeTicks() here
  // because many places have DCHECKs for !time.is_null(), which would be hit if
  // we set the clock to be exactly 0.
  AdvanceClock(-CurrentTimeTicksInSeconds() + 0.0001);
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  std::vector<v8::Local<v8::ArrayBuffer>> objects;
  objects.push_back(v8::ArrayBuffer::New(isolate, 100));

  MemoryInfo* precise_memory =
      MemoryInfo::Create(MemoryInfo::Precision::Precise);
  CheckValues(precise_memory, MemoryInfo::Precision::Precise);
  EXPECT_LT(0u, precise_memory->usedJSHeapSize());
  EXPECT_LT(0u, precise_memory->totalJSHeapSize());
  EXPECT_LT(0u, precise_memory->jsHeapSizeLimit());
}

}  // namespace blink
