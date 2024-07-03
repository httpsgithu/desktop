// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

#include <sstream>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/limits.h"
#include "media/base/mock_filters.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/media_buildflags.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediarecorder/fake_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/openh264/openh264_buildflags.h"
#include "ui/gfx/gpu_memory_buffer.h"

using video_track_recorder::kVEAEncoderMinResolutionHeight;
using video_track_recorder::kVEAEncoderMinResolutionWidth;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::ValuesIn;

namespace blink {
namespace {

// Specifies frame type for test.
enum class TestFrameType {
  kNv12GpuMemoryBuffer,  // Implies media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER
  kNv12Software,         // Implies media::VideoFrame::STORAGE_OWNED_MEMORY
  kI420                  // Implies media::VideoFrame::STORAGE_OWNED_MEMORY
};

const TestFrameType kTestFrameTypes[] = {TestFrameType::kNv12GpuMemoryBuffer,
                                         TestFrameType::kNv12Software,
                                         TestFrameType::kI420};

const VideoTrackRecorder::CodecId kTrackRecorderTestCodec[] = {
    VideoTrackRecorder::CodecId::kVp8,
    VideoTrackRecorder::CodecId::kVp9,
#if BUILDFLAG(ENABLE_OPENH264) || BUILDFLAG(ENABLE_EXTERNAL_OPENH264) || \
    BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
    VideoTrackRecorder::CodecId::kH264,
#endif
#if BUILDFLAG(ENABLE_LIBAOM)
    VideoTrackRecorder::CodecId::kAv1,
#endif
};
const gfx::Size kTrackRecorderTestSize[] = {
    gfx::Size(kVEAEncoderMinResolutionWidth / 2,
              kVEAEncoderMinResolutionHeight / 2),
    gfx::Size(kVEAEncoderMinResolutionWidth, kVEAEncoderMinResolutionHeight)};
static const int kTrackRecorderTestSizeDiff = 20;

constexpr media::VideoCodec MediaVideoCodecFromCodecId(
    VideoTrackRecorder::CodecId id) {
  switch (id) {
    case VideoTrackRecorder::CodecId::kVp8:
      return media::VideoCodec::kVP8;
    case VideoTrackRecorder::CodecId::kVp9:
      return media::VideoCodec::kVP9;
// Note: The H264 tests in this file are written explicitly for OpenH264 and
// will fail for hardware encoders that aren't 1 in 1 out.
#if BUILDFLAG(ENABLE_OPENH264) || BUILDFLAG(ENABLE_EXTERNAL_OPENH264) || \
    BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
    case VideoTrackRecorder::CodecId::kH264:
      return media::VideoCodec::kH264;
#endif
#if BUILDFLAG(ENABLE_LIBAOM)
    case VideoTrackRecorder::CodecId::kAv1:
      return media::VideoCodec::kAV1;
#endif
    default:
      return media::VideoCodec::kUnknown;
  }
  NOTREACHED() << "Unsupported video codec";
  return media::VideoCodec::kUnknown;
}

media::VideoCodecProfile MediaVideoCodecProfileFromCodecId(
    VideoTrackRecorder::CodecId id) {
  switch (id) {
    case VideoTrackRecorder::CodecId::kVp8:
      return media::VideoCodecProfile::VP8PROFILE_ANY;
    case VideoTrackRecorder::CodecId::kVp9:
      return media::VideoCodecProfile::VP9PROFILE_PROFILE0;
// Note: The H264 tests in this file are written explicitly for OpenH264 and
// will fail for hardware encoders that aren't 1 in 1 out.
#if BUILDFLAG(ENABLE_OPENH264) || BUILDFLAG(ENABLE_EXTERNAL_OPENH264) || \
    BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
    case VideoTrackRecorder::CodecId::kH264:
      return media::VideoCodecProfile::H264PROFILE_MIN;
#endif
#if BUILDFLAG(ENABLE_LIBAOM)
    case VideoTrackRecorder::CodecId::kAv1:
      return media::VideoCodecProfile::AV1PROFILE_MIN;
#endif
    default:
      break;
  }
  NOTREACHED() << "Unsupported video codec";
  return media::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
}

bool ShouldSkipTestRequiringSyncOperation(
    VideoTrackRecorder::CodecId codec_id) {
#if BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
  // VideoTrackRecorderImpl initializes the encoder upon the delivery of the
  // first frame. The test contains assumptions that this is fully
  // synchronous. However, for external OpenH264, we must go through the VEA
  // layer, which means RequireBitstreamBuffers() doesn't arrive in time for
  // VEAEncoder to perform the encodes as expected.
  return codec_id == VideoTrackRecorder::CodecId::kH264;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
}

bool VideoEncoderQueuesInput(VideoTrackRecorder::CodecId codec_id) {
#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
  return codec_id == VideoTrackRecorder::CodecId::kH264;
#else
  return false;
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
}

}  // namespace

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MockTestingPlatform : public IOTaskRunnerTestingPlatformSupport {
 public:
  MockTestingPlatform() = default;
  ~MockTestingPlatform() override = default;

  MOCK_METHOD(media::GpuVideoAcceleratorFactories*,
              GetGpuFactories,
              (),
              (override));
#if BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
  MOCK_METHOD(media::GpuVideoAcceleratorFactories*,
              GetExternalSoftwareFactories,
              (),
              (override));
#endif  // BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
};

class MockVideoTrackRecorderCallbackInterface
    : public GarbageCollected<MockVideoTrackRecorderCallbackInterface>,
      public VideoTrackRecorder::CallbackInterface {
 public:
  virtual ~MockVideoTrackRecorderCallbackInterface() = default;
  MOCK_METHOD(void,
              OnPassthroughVideo,
              (const media::Muxer::VideoParameters& params,
               std::string encoded_data,
               std::string encoded_alpha,
               base::TimeTicks timestamp,
               bool is_key_frame),
              (override));
  MOCK_METHOD(
      void,
      OnEncodedVideo,
      (const media::Muxer::VideoParameters& params,
       std::string encoded_data,
       std::string encoded_alpha,
       std::optional<media::VideoEncoder::CodecDescription> codec_description,
       base::TimeTicks timestamp,
       bool is_key_frame),
      (override));
  MOCK_METHOD(std::unique_ptr<media::VideoEncoderMetricsProvider>,
              CreateVideoEncoderMetricsProvider,
              (),
              (override));

  MOCK_METHOD(void, OnVideoEncodingError, (), (override));
  MOCK_METHOD(void, OnSourceReadyStateChanged, (), (override));
  void Trace(Visitor*) const override {}
};

class VideoTrackRecorderTestBase {
 public:
  VideoTrackRecorderTestBase()
      : mock_callback_interface_(
            MakeGarbageCollected<MockVideoTrackRecorderCallbackInterface>()) {
    ON_CALL(*mock_callback_interface_, CreateVideoEncoderMetricsProvider())
        .WillByDefault(
            ::testing::Invoke(this, &VideoTrackRecorderTestBase::
                                        CreateMockVideoEncoderMetricsProvider));
  }

 protected:
  virtual ~VideoTrackRecorderTestBase() {
    mock_callback_interface_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  std::unique_ptr<media::VideoEncoderMetricsProvider>
  CreateMockVideoEncoderMetricsProvider() {
    return std::make_unique<media::MockVideoEncoderMetricsProvider>();
  }

  test::TaskEnvironment task_environment_;
  Persistent<MockVideoTrackRecorderCallbackInterface> mock_callback_interface_;
};

class VideoTrackRecorderTest : public VideoTrackRecorderTestBase {
 public:
  VideoTrackRecorderTest() : mock_source_(new MockMediaStreamVideoSource()) {
    const String track_id("dummy");
    source_ = MakeGarbageCollected<MediaStreamSource>(
        track_id, MediaStreamSource::kTypeVideo, track_id, false /*remote*/,
        base::WrapUnique(mock_source_.get()));
    EXPECT_CALL(*mock_source_, OnRequestRefreshFrame())
        .Times(testing::AnyNumber());
    EXPECT_CALL(*mock_source_, OnCapturingLinkSecured(_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(*mock_source_, GetSubCaptureTargetVersion())
        .Times(testing::AnyNumber())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_source_, OnSourceCanDiscardAlpha(_))
        .Times(testing::AnyNumber());

    auto platform_track = std::make_unique<MediaStreamVideoTrack>(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        true /* enabled */);
    track_ = platform_track.get();
    component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        source_, std::move(platform_track));

    // Paranoia checks.
    EXPECT_EQ(component_->Source()->GetPlatformSource(),
              source_->GetPlatformSource());
    EXPECT_TRUE(scheduler::GetSingleThreadTaskRunnerForTesting()
                    ->BelongsToCurrentThread());

    EXPECT_CALL(*platform_, GetGpuFactories())
        .Times(testing::AnyNumber())
        .WillRepeatedly(Return(nullptr));
  }

  VideoTrackRecorderTest(const VideoTrackRecorderTest&) = delete;
  VideoTrackRecorderTest& operator=(const VideoTrackRecorderTest&) = delete;

  ~VideoTrackRecorderTest() override {
    component_ = nullptr;
    source_ = nullptr;
    video_track_recorder_.reset();
  }

  void InitializeRecorder(
      VideoTrackRecorder::CodecId codec_id,
      KeyFrameRequestProcessor::Configuration keyframe_config =
          KeyFrameRequestProcessor::Configuration()) {
    InitializeRecorder(VideoTrackRecorder::CodecProfile(codec_id),
                       keyframe_config);
  }

  void InitializeRecorder(
      VideoTrackRecorder::CodecProfile codec_profile,
      KeyFrameRequestProcessor::Configuration keyframe_config =
          KeyFrameRequestProcessor::Configuration()) {
    video_track_recorder_ = std::make_unique<VideoTrackRecorderImpl>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), codec_profile,
        WebMediaStreamTrack(component_.Get()), mock_callback_interface_,
        /*bits_per_second=*/1000000, keyframe_config);
  }

  void Encode(scoped_refptr<media::VideoFrame> frame,
              base::TimeTicks capture_time,
              bool allow_vea_encoder = true) {
    EXPECT_TRUE(scheduler::GetSingleThreadTaskRunnerForTesting()
                    ->BelongsToCurrentThread());
    video_track_recorder_->OnVideoFrameForTesting(
        std::move(frame), capture_time, allow_vea_encoder);
  }

  void OnFailed() { FAIL(); }
  void OnError() { video_track_recorder_->OnHardwareEncoderError(); }

  bool CanEncodeAlphaChannel() {
    bool result;
    base::WaitableEvent finished;
    video_track_recorder_->encoder_.PostTaskWithThisObject(CrossThreadBindOnce(
        [](base::WaitableEvent* finished, bool* out_result,
           VideoTrackRecorder::Encoder* encoder) {
          *out_result = encoder->CanEncodeAlphaChannel();
          finished->Signal();
        },
        CrossThreadUnretained(&finished), CrossThreadUnretained(&result)));
    finished.Wait();
    return result;
  }

  bool IsScreenContentEncoding() {
    bool result;
    base::WaitableEvent finished;
    video_track_recorder_->encoder_.PostTaskWithThisObject(CrossThreadBindOnce(
        [](base::WaitableEvent* finished, bool* out_result,
           VideoTrackRecorder::Encoder* encoder) {
          *out_result = encoder->IsScreenContentEncodingForTesting();
          finished->Signal();
        },
        CrossThreadUnretained(&finished), CrossThreadUnretained(&result)));
    finished.Wait();
    return result;
  }

  bool HasEncoderInstance() const {
    return !video_track_recorder_->encoder_.is_null();
  }

  // We don't have access to the encoder's Flush() method here directly, so we
  // invoke it indirectly through a resolution size change. This is needed for
  // encoders that might buffer some amount of input before emitting output.
  void MaybeForceFlush(
      const media::VideoFrame& last_frame,
      VideoTrackRecorder::CodecId codec_id,
      TestFrameType frame_type,
      media::MockVideoEncoderMetricsProvider* mock_metrics_provider = nullptr) {
#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
    if (!VideoEncoderQueuesInput(codec_id)) {
      return;
    }

    gfx::Size frame_size = last_frame.coded_size();
    frame_size.Enlarge(kTrackRecorderTestSizeDiff, 0);

    const bool encode_alpha_channel = !media::IsOpaque(last_frame.format());

    scoped_refptr<media::VideoFrame> video_frame = CreateFrameForTest(
        frame_type, frame_size, encode_alpha_channel, /*padding=*/0);

    const auto now = base::TimeTicks::Now();
    if (mock_metrics_provider) {
      EXPECT_CALL(*mock_metrics_provider, MockInitialize(_, _, _, _));
    } else {
      EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, now, _))
          .Times(testing::AnyNumber());
    }
    Encode(std::move(video_frame), now);
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS) && BUILDFLAG(IS_MAC)
  }

  ScopedTestingPlatformSupport<MockTestingPlatform> platform_;

  // All members are non-const due to the series of initialize() calls needed.
  // |mock_source_| is owned by |source_|, |track_| by |component_|.
  raw_ptr<MockMediaStreamVideoSource> mock_source_;
  Persistent<MediaStreamSource> source_;
  raw_ptr<MediaStreamVideoTrack> track_;
  Persistent<MediaStreamComponent> component_;

  std::unique_ptr<VideoTrackRecorderImpl> video_track_recorder_;

 protected:
  scoped_refptr<media::VideoFrame> CreateFrameForTest(
      TestFrameType frame_type,
      const gfx::Size& frame_size,
      bool encode_alpha_channel,
      int padding) {
    const gfx::Size padded_size(frame_size.width() + padding,
                                frame_size.height());
    if (frame_type == TestFrameType::kI420) {
      return media::VideoFrame::CreateZeroInitializedFrame(
          encode_alpha_channel ? media::PIXEL_FORMAT_I420A
                               : media::PIXEL_FORMAT_I420,
          padded_size, gfx::Rect(frame_size), frame_size, base::TimeDelta());
    }

    scoped_refptr<media::VideoFrame> video_frame = blink::CreateTestFrame(
        padded_size, gfx::Rect(frame_size), frame_size,
        frame_type == TestFrameType::kNv12Software
            ? media::VideoFrame::STORAGE_OWNED_MEMORY
            : media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
        media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
    scoped_refptr<media::VideoFrame> video_frame2 = video_frame;
    if (frame_type == TestFrameType::kNv12GpuMemoryBuffer)
      video_frame2 = media::ConvertToMemoryMappedFrame(video_frame);

    // Fade to black.
    const uint8_t kBlackY = 0x00;
    const uint8_t kBlackUV = 0x80;
    memset(video_frame2->writable_data(0), kBlackY,
           video_frame2->stride(0) * frame_size.height());
    memset(video_frame2->writable_data(1), kBlackUV,
           video_frame2->stride(1) * (frame_size.height() / 2));
    if (frame_type == TestFrameType::kNv12GpuMemoryBuffer)
      return video_frame;
    return video_frame2;
  }
};

class VideoTrackRecorderTestWithAllCodecs : public ::testing::Test,
                                            public VideoTrackRecorderTest {
 public:
  VideoTrackRecorderTestWithAllCodecs() = default;
  ~VideoTrackRecorderTestWithAllCodecs() override = default;
};

TEST_F(VideoTrackRecorderTestWithAllCodecs, NoCrashInConfigureEncoder) {
  constexpr std::pair<VideoTrackRecorder::CodecId, bool> kCodecIds[] = {
      {VideoTrackRecorder::CodecId::kVp8, true},
      {VideoTrackRecorder::CodecId::kVp9, true},
#if BUILDFLAG(USE_PROPRIETARY_CODECS) ||   \
    BUILDFLAG(ENABLE_EXTERNAL_OPENH264) || \
    BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
      {VideoTrackRecorder::CodecId::kH264,
#if BUILDFLAG(ENABLE_OPENH264) || BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS) || \
    BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
       true
#else
       false
#endif  // BUILDFLAG(ENABLE_OPENH264) ||
        // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS) ||
        // BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
      },
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS) ||
        // BUILDFLAG(ENABLE_EXTERNAL_OPENH264) ||
        // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
      {VideoTrackRecorder::CodecId::kAv1,
#if BUILDFLAG(ENABLE_LIBAOM)
       true
#else
       false
#endif  // BUILDFLAG(ENABLE_LIBAOM)
      },
  };
  for (auto [codec_id, can_sw_encode] : kCodecIds) {
    if (ShouldSkipTestRequiringSyncOperation(codec_id)) {
      continue;
    }
    InitializeRecorder(codec_id);
    const scoped_refptr<media::VideoFrame> video_frame =
        CreateFrameForTest(TestFrameType::kI420,
                           gfx::Size(kVEAEncoderMinResolutionWidth,
                                     kVEAEncoderMinResolutionHeight),
                           /*encode_alpha_channel=*/false, /*padding=*/0);
    if (!video_frame) {
      ASSERT_TRUE(!!video_frame);
    }
    base::RunLoop run_loop;
    InSequence s;
    if (can_sw_encode) {
      EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo)
          .WillOnce(RunClosure(run_loop.QuitClosure()));
    } else {
      EXPECT_CALL(*mock_callback_interface_, OnVideoEncodingError)
          .WillOnce(RunClosure(run_loop.QuitClosure()));
    }
    Encode(video_frame, base::TimeTicks::Now());
    MaybeForceFlush(*video_frame, codec_id, TestFrameType::kI420);
    run_loop.Run();
    EXPECT_EQ(HasEncoderInstance(), can_sw_encode);
  }
}

class VideoTrackRecorderTestWithCodec
    : public TestWithParam<testing::tuple<VideoTrackRecorder::CodecId, bool>>,
      public VideoTrackRecorderTest,
      public ScopedMediaRecorderUseMediaVideoEncoderForTest {
 public:
  VideoTrackRecorderTestWithCodec()
      : ScopedMediaRecorderUseMediaVideoEncoderForTest(
            testing::get<1>(GetParam())) {}
  ~VideoTrackRecorderTestWithCodec() override = default;
};

// Construct and destruct all objects, in particular |video_track_recorder_| and
// its inner object(s). This is a non trivial sequence.
TEST_P(VideoTrackRecorderTestWithCodec, ConstructAndDestruct) {
  InitializeRecorder(testing::get<0>(GetParam()));
}

// Initializes an encoder with very large frame that causes an error on the
// initialization. Check if the error is reported via OnVideoEncodingError().
TEST_P(VideoTrackRecorderTestWithCodec,
       SoftwareEncoderInitializeErrorWithLargeFrame) {
  if (ShouldSkipTestRequiringSyncOperation(testing::get<0>(GetParam()))) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  const VideoTrackRecorder::CodecId codec_id = testing::get<0>(GetParam());
  if (codec_id == VideoTrackRecorder::CodecId::kVp9
#if BUILDFLAG(ENABLE_LIBAOM)
      || codec_id == VideoTrackRecorder::CodecId::kAv1
#endif
  ) {
    // The max bits on width and height are 16bits in VP9 and AV1. Since it is
    // more than media::limits::kMaxDimension (15 bits), the larger frame
    // causing VP9 and AV1 initialization cannot be created because
    // CreateBlackFrame() fails.
    GTEST_SKIP();
  }
  InitializeRecorder(codec_id);
  constexpr gfx::Size kTooLargeResolution(media::limits::kMaxDimension - 1, 1);
  auto too_large_frame =
      media::VideoFrame::CreateBlackFrame(kTooLargeResolution);
  ASSERT_TRUE(too_large_frame);
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_callback_interface_, OnVideoEncodingError)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  Encode(too_large_frame, base::TimeTicks::Now());
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoTrackRecorderTestWithCodec,
                         ::testing::Combine(ValuesIn(kTrackRecorderTestCodec),
                                            ::testing::Bool()));

// TODO(crbug/1177593): refactor the test parameter space to something more
// reasonable. Many tests below ignore parts of the space leading to too much
// being tested.
class VideoTrackRecorderTestParam
    : public TestWithParam<testing::tuple<VideoTrackRecorder::CodecId,
                                          gfx::Size,
                                          bool,
                                          TestFrameType,
                                          bool>>,
      public VideoTrackRecorderTest,
      public ScopedMediaRecorderUseMediaVideoEncoderForTest {
 public:
  VideoTrackRecorderTestParam()
      : ScopedMediaRecorderUseMediaVideoEncoderForTest(
            testing::get<4>(GetParam())) {}
  ~VideoTrackRecorderTestParam() override = default;

  static bool ShouldSkipTestRequiringSyncOperation() {
    return ::blink::ShouldSkipTestRequiringSyncOperation(
        testing::get<0>(GetParam()));
  }

  static bool VideoEncoderQueuesInput() {
    return ::blink::VideoEncoderQueuesInput(
        testing::get<VideoTrackRecorder::CodecId>(GetParam()));
  }

  void MaybeForceFlush(
      const media::VideoFrame& last_frame,
      media::MockVideoEncoderMetricsProvider* mock_metrics_provider = nullptr) {
    VideoTrackRecorderTest::MaybeForceFlush(
        last_frame, testing::get<VideoTrackRecorder::CodecId>(GetParam()),
        testing::get<TestFrameType>(GetParam()), mock_metrics_provider);
  }
};

// Creates the encoder and encodes 2 frames of the same size; the encoder
// should be initialised and produce a keyframe, then a non-keyframe. Finally
// a frame of larger size is sent and is expected to be encoded as a keyframe.
// If |encode_alpha_channel| is enabled, encoder is expected to return a
// second output with encoded alpha data.
TEST_P(VideoTrackRecorderTestParam, VideoEncoding) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  InitializeRecorder(testing::get<0>(GetParam()));

  const bool encode_alpha_channel = testing::get<2>(GetParam());
  // |frame_size| cannot be arbitrarily small, should be reasonable.
  const gfx::Size& frame_size = testing::get<1>(GetParam());
  const TestFrameType test_frame_type = testing::get<3>(GetParam());

  // We don't support alpha channel with GpuMemoryBuffer frames.
  if (test_frame_type != TestFrameType::kI420 && encode_alpha_channel) {
    return;
  }

  const scoped_refptr<media::VideoFrame> video_frame = CreateFrameForTest(
      test_frame_type, frame_size, encode_alpha_channel, /*padding=*/0);
  if (!video_frame)
    ASSERT_TRUE(!!video_frame);

  const double kFrameRate = 60.0f;
  video_frame->metadata().frame_rate = kFrameRate;

  InSequence s;
  const base::TimeTicks timeticks_now = base::TimeTicks::Now();
  base::StringPiece first_frame_encoded_data;
  base::StringPiece first_frame_encoded_alpha;
  EXPECT_CALL(*mock_callback_interface_,
              OnEncodedVideo(_, _, _, _, timeticks_now, true))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&first_frame_encoded_data),
                      SaveArg<2>(&first_frame_encoded_alpha)));

  const base::TimeTicks timeticks_later = base::TimeTicks::Now();
  base::StringPiece second_frame_encoded_data;
  base::StringPiece second_frame_encoded_alpha;
  EXPECT_CALL(*mock_callback_interface_,
              OnEncodedVideo(_, _, _, _, timeticks_later, false))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&second_frame_encoded_data),
                      SaveArg<2>(&second_frame_encoded_alpha)));

  const gfx::Size frame_size2(frame_size.width() + kTrackRecorderTestSizeDiff,
                              frame_size.height());
  const scoped_refptr<media::VideoFrame> video_frame2 = CreateFrameForTest(
      test_frame_type, frame_size2, encode_alpha_channel, /*padding=*/0);

  base::RunLoop run_loop;

  base::StringPiece third_frame_encoded_data;
  base::StringPiece third_frame_encoded_alpha;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&third_frame_encoded_data),
                      SaveArg<2>(&third_frame_encoded_alpha),
                      RunClosure(run_loop.QuitClosure())));
  // A test-only TSAN problem is fixed by placing the encodes down here and not
  // close to the expectation setups.
  Encode(video_frame, timeticks_now);
  Encode(video_frame, timeticks_later);
  Encode(video_frame2, base::TimeTicks::Now());
  MaybeForceFlush(*video_frame2);

  run_loop.Run();

  const size_t kEncodedSizeThreshold = 14;
  EXPECT_GE(first_frame_encoded_data.size(), kEncodedSizeThreshold);
  EXPECT_GE(second_frame_encoded_data.size(), kEncodedSizeThreshold);
  EXPECT_GE(third_frame_encoded_data.size(), kEncodedSizeThreshold);

  // We only support NV12 with GpuMemoryBuffer video frame.
  if (test_frame_type == TestFrameType::kI420 && encode_alpha_channel &&
      CanEncodeAlphaChannel()) {
    EXPECT_GE(first_frame_encoded_alpha.size(), kEncodedSizeThreshold);
    EXPECT_GE(second_frame_encoded_alpha.size(), kEncodedSizeThreshold);
    EXPECT_GE(third_frame_encoded_alpha.size(), kEncodedSizeThreshold);
  } else {
    const size_t kEmptySize = 0;
    EXPECT_EQ(first_frame_encoded_alpha.size(), kEmptySize);
    EXPECT_EQ(second_frame_encoded_alpha.size(), kEmptySize);
    EXPECT_EQ(third_frame_encoded_alpha.size(), kEmptySize);
  }

  // The encoder is configured non screen content by default.
  EXPECT_FALSE(IsScreenContentEncoding());

  Mock::VerifyAndClearExpectations(this);
}

// VideoEncoding with the screencast track.
TEST_P(VideoTrackRecorderTestParam, ConfigureEncoderWithScreenContent) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  track_->SetIsScreencastForTesting(true);

  InitializeRecorder(testing::get<0>(GetParam()));

  const bool encode_alpha_channel = testing::get<2>(GetParam());
  // |frame_size| cannot be arbitrarily small, should be reasonable.
  const gfx::Size& frame_size = testing::get<1>(GetParam());
  const TestFrameType test_frame_type = testing::get<3>(GetParam());

  // We don't support alpha channel with GpuMemoryBuffer frames.
  if (test_frame_type != TestFrameType::kI420 && encode_alpha_channel) {
    return;
  }

  const scoped_refptr<media::VideoFrame> video_frame = CreateFrameForTest(
      test_frame_type, frame_size, encode_alpha_channel, /*padding=*/0);
  if (!video_frame) {
    ASSERT_TRUE(!!video_frame);
  }

  InSequence s;
  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo)
      .WillOnce(RunClosure(run_loop1.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  MaybeForceFlush(*video_frame);
  run_loop1.Run();

  EXPECT_TRUE(HasEncoderInstance());

  // MediaRecorderEncoderWrapper is configured with a screen content hint.
  const bool is_media_recorder_encoder_wrapper =
      testing::get<4>(GetParam()) ||
      testing::get<0>(GetParam()) == VideoTrackRecorder::CodecId::kAv1
#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS) && \
    !BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
      || testing::get<0>(GetParam()) == VideoTrackRecorder::CodecId::kH264
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS) && \
        // !BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
      ;
  EXPECT_EQ(is_media_recorder_encoder_wrapper, IsScreenContentEncoding());
  Mock::VerifyAndClearExpectations(this);
}

// Same as VideoEncoding but add the EXPECT_CALL for the
// VideoEncoderMetricsProvider.
TEST_P(VideoTrackRecorderTestParam, CheckMetricsProviderInVideoEncoding) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  InitializeRecorder(testing::get<0>(GetParam()));

  const bool encode_alpha_channel = testing::get<2>(GetParam());
  // |frame_size| cannot be arbitrarily small, should be reasonable.
  const gfx::Size& frame_size = testing::get<1>(GetParam());
  const TestFrameType test_frame_type = testing::get<3>(GetParam());

  // We don't support alpha channel with GpuMemoryBuffer frames.
  if (test_frame_type != TestFrameType::kI420 && encode_alpha_channel) {
    return;
  }

  const media::VideoCodecProfile video_codec_profile =
      MediaVideoCodecProfileFromCodecId(testing::get<0>(GetParam()));

  auto metrics_provider =
      std::make_unique<media::MockVideoEncoderMetricsProvider>();
  media::MockVideoEncoderMetricsProvider* mock_metrics_provider =
      metrics_provider.get();
  int initialize_time = 1;
  if (encode_alpha_channel &&
      (video_codec_profile == media::VideoCodecProfile::VP8PROFILE_ANY ||
       video_codec_profile == media::VideoCodecProfile::VP9PROFILE_PROFILE0) &&
      !testing::get<4>(GetParam())) {
    initialize_time = 2;
  }

  base::RunLoop run_loop1;
  InSequence s;
  EXPECT_CALL(*mock_callback_interface_, CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(::testing::ByMove(std::move(metrics_provider))));
  EXPECT_CALL(*mock_metrics_provider,
              MockInitialize(video_codec_profile, frame_size,
                             /*is_hardware_encoder=*/false,
                             media::SVCScalabilityMode::kL1T1))
      .Times(initialize_time);
  EXPECT_CALL(*mock_metrics_provider, MockIncrementEncodedFrameCount())
      .Times(2);

  const gfx::Size frame_size2(frame_size.width() + kTrackRecorderTestSizeDiff,
                              frame_size.height());
  EXPECT_CALL(*mock_metrics_provider,
              MockInitialize(video_codec_profile, frame_size2,
                             /*is_hardware_encoder=*/false,
                             media::SVCScalabilityMode::kL1T1))
      .Times(initialize_time);
  EXPECT_CALL(*mock_metrics_provider, MockIncrementEncodedFrameCount())
      .WillOnce(RunClosure(run_loop1.QuitClosure()));

  const scoped_refptr<media::VideoFrame> video_frame = CreateFrameForTest(
      test_frame_type, frame_size, encode_alpha_channel, /*padding=*/0);

  const double kFrameRate = 60.0f;
  video_frame->metadata().frame_rate = kFrameRate;
  const scoped_refptr<media::VideoFrame> video_frame2 = CreateFrameForTest(
      test_frame_type, frame_size2, encode_alpha_channel, /*padding=*/0);

  const base::TimeTicks timeticks_now = base::TimeTicks::Now();
  const base::TimeTicks timeticks_later =
      timeticks_now + base::Milliseconds(10);
  const base::TimeTicks timeticks_last =
      timeticks_later + base::Milliseconds(10);

  // A test-only TSAN problem is fixed by placing the encodes down here and not
  // close to the expectation setups.
  Encode(video_frame, timeticks_now);
  Encode(video_frame, timeticks_later);
  Encode(video_frame2, timeticks_last);
  MaybeForceFlush(*video_frame2, mock_metrics_provider);

  run_loop1.Run();

  // Since |encoder_| is destroyed on the encoder sequence checker, it and the
  // MockVideoEncoderMetricsProvider are asynchronously. It causes the leak
  // mock object, |mock_metrics_provider|. Avoid it by waiting until the
  // mock object is destroyed.
  base::RunLoop run_loop2;

  EXPECT_CALL(*mock_metrics_provider, MockDestroy())
      .WillOnce(RunClosure(run_loop2.QuitClosure()));
  video_track_recorder_.reset();
  run_loop2.Run();

  Mock::VerifyAndClearExpectations(this);
}

// Inserts a frame which has different coded size than the visible rect and
// expects encode to be completed without raising any sanitizer flags.
TEST_P(VideoTrackRecorderTestParam, EncodeFrameWithPaddedCodedSize) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  InitializeRecorder(testing::get<0>(GetParam()));

  const gfx::Size& frame_size = testing::get<1>(GetParam());
  const size_t kCodedSizePadding = 16;
  const TestFrameType test_frame_type = testing::get<3>(GetParam());
  scoped_refptr<media::VideoFrame> video_frame =
      CreateFrameForTest(test_frame_type, frame_size,
                         /*encode_alpha_channel=*/false, kCodedSizePadding);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .Times(1)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  MaybeForceFlush(*video_frame);
  run_loop.Run();

  Mock::VerifyAndClearExpectations(this);
}

TEST_P(VideoTrackRecorderTestParam, EncodeFrameRGB) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  InitializeRecorder(testing::get<0>(GetParam()));

  const gfx::Size& frame_size = testing::get<1>(GetParam());

  // TODO(crbug/1177593): Refactor test harness to use a cleaner parameter
  // space.
  // Let kI420 indicate owned memory, and kNv12GpuMemoryBuffer to indicate GMB
  // storage. Don't test for kNv12Software.
  const TestFrameType test_frame_type = testing::get<3>(GetParam());
  if (test_frame_type == TestFrameType::kNv12Software)
    return;

  const bool encode_alpha_channel = testing::get<2>(GetParam());
  media::VideoPixelFormat pixel_format = encode_alpha_channel
                                             ? media::PIXEL_FORMAT_ARGB
                                             : media::PIXEL_FORMAT_XRGB;
  scoped_refptr<media::VideoFrame> video_frame =
      test_frame_type == TestFrameType::kI420
          ? media::VideoFrame::CreateZeroInitializedFrame(
                pixel_format, frame_size, gfx::Rect(frame_size), frame_size,
                base::TimeDelta())
          : blink::CreateTestFrame(frame_size, gfx::Rect(frame_size),
                                   frame_size,
                                   media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                                   pixel_format, base::TimeDelta());

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .Times(1)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  MaybeForceFlush(*video_frame);
  run_loop.Run();

  Mock::VerifyAndClearExpectations(this);
}

TEST_P(VideoTrackRecorderTestParam, EncoderHonorsKeyFrameRequests) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }
  if (VideoEncoderQueuesInput()) {
    GTEST_SKIP() << "Due to the way this test uses Gmock and RunLoops, "
                    "flushing is required for encoders that queue input. "
                    "Flushing implies subsequent key frame generation, and "
                    "that renders this test pointless.";
  }

  InitializeRecorder(testing::get<0>(GetParam()));
  InSequence s;
  auto frame = media::VideoFrame::CreateBlackFrame(kTrackRecorderTestSize[0]);

  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo)
      .WillOnce(RunClosure(run_loop1.QuitClosure()));
  Encode(frame, base::TimeTicks::Now());
  run_loop1.Run();

  // Request the next frame to be a key frame, and the following frame a delta
  // frame.
  video_track_recorder_->ForceKeyFrameForNextFrameForTesting();
  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true));
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false))
      .WillOnce(RunClosure(run_loop2.QuitClosure()));
  Encode(frame, base::TimeTicks::Now());
  Encode(frame, base::TimeTicks::Now());
  run_loop2.Run();

  Mock::VerifyAndClearExpectations(this);
}

TEST_P(VideoTrackRecorderTestParam,
       NoSubsequenceKeyFramesWithDefaultKeyFrameConfig) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  InitializeRecorder(testing::get<0>(GetParam()));
  auto frame = media::VideoFrame::CreateBlackFrame(kTrackRecorderTestSize[0]);

  auto origin = base::TimeTicks::Now();
  InSequence s;
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true));
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false))
      .Times(8);
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  for (int i = 0; i != 10; ++i) {
    Encode(frame, origin + i * base::Minutes(1));
  }
  MaybeForceFlush(*frame);
  run_loop.Run();
}

TEST_P(VideoTrackRecorderTestParam, KeyFramesGeneratedWithIntervalCount) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  // Configure 3 delta frames for every key frame.
  InitializeRecorder(testing::get<0>(GetParam()), /*keyframe_config=*/3u);
  auto frame = media::VideoFrame::CreateBlackFrame(kTrackRecorderTestSize[0]);

  auto origin = base::TimeTicks::Now();
  InSequence s;
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true));
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false))
      .Times(3);
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true));
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false))
      .Times(2);
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  for (int i = 0; i != 8; ++i) {
    Encode(frame, origin);
  }
  MaybeForceFlush(*frame);
  run_loop.Run();
}

TEST_P(VideoTrackRecorderTestParam, KeyFramesGeneratedWithIntervalDuration) {
  if (ShouldSkipTestRequiringSyncOperation()) {
    GTEST_SKIP() << "Cannot use external OpnH264 encoder here";
  }

  // Configure 1 key frame every 2 secs.
  InitializeRecorder(testing::get<0>(GetParam()),
                     /*keyframe_config=*/base::Seconds(2));
  auto frame = media::VideoFrame::CreateBlackFrame(kTrackRecorderTestSize[0]);

  InSequence s;
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true));
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false));
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true));
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false));
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  auto origin = base::TimeTicks();
  Encode(frame, origin);                             // Key frame emitted.
  Encode(frame, origin + base::Milliseconds(1000));  //
  Encode(frame, origin + base::Milliseconds(2100));  // Key frame emitted.
  Encode(frame, origin + base::Milliseconds(4099));  //
  Encode(frame, origin + base::Milliseconds(4100));  // Key frame emitted.
  MaybeForceFlush(*frame);
  run_loop.Run();
}

std::string PrintTestParams(
    const testing::TestParamInfo<testing::tuple<VideoTrackRecorder::CodecId,
                                                gfx::Size,
                                                bool,
                                                TestFrameType,
                                                bool>>& info) {
  std::stringstream ss;
  ss << "codec ";
  switch (testing::get<0>(info.param)) {
    case VideoTrackRecorder::CodecId::kVp8:
      ss << "vp8";
      break;
    case VideoTrackRecorder::CodecId::kVp9:
      ss << "vp9";
      break;
#if BUILDFLAG(ENABLE_OPENH264) || BUILDFLAG(ENABLE_EXTERNAL_OPENH264) || \
    BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
    case VideoTrackRecorder::CodecId::kH264:
      ss << "h264";
      break;
#endif
#if BUILDFLAG(ENABLE_LIBAOM)
    case VideoTrackRecorder::CodecId::kAv1:
      ss << "av1";
      break;
#endif
    case VideoTrackRecorder::CodecId::kLast:
    default:
      ss << "invalid";
      break;
  }

  ss << " size " + testing::get<1>(info.param).ToString() << " encode alpha "
     << (testing::get<2>(info.param) ? "true" : "false") << " frame type ";
  switch (testing::get<3>(info.param)) {
    case TestFrameType::kNv12GpuMemoryBuffer:
      ss << "NV12 GMB";
      break;
    case TestFrameType::kNv12Software:
      ss << "I420 SW";
      break;
    case TestFrameType::kI420:
      ss << "I420";
      break;
  }
  ss << " mediaVideoEncoder "
     << (testing::get<4>(info.param) ? "true" : "false");

  std::string out;
  base::ReplaceChars(ss.str(), " ", "_", &out);
  return out;
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoTrackRecorderTestParam,
                         ::testing::Combine(ValuesIn(kTrackRecorderTestCodec),
                                            ValuesIn(kTrackRecorderTestSize),
                                            ::testing::Bool(),
                                            ValuesIn(kTestFrameTypes),
                                            ::testing::Bool()),
                         PrintTestParams);

class VideoTrackRecorderTestMediaVideoEncoderParam
    : public ::testing::TestWithParam<bool>,
      public VideoTrackRecorderTest,
      public ScopedMediaRecorderUseMediaVideoEncoderForTest {
 public:
  VideoTrackRecorderTestMediaVideoEncoderParam()
      : ScopedMediaRecorderUseMediaVideoEncoderForTest(GetParam()) {}
  ~VideoTrackRecorderTestMediaVideoEncoderParam() override = default;
};

TEST_P(VideoTrackRecorderTestMediaVideoEncoderParam, RelaysReadyStateEnded) {
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_callback_interface_, OnSourceReadyStateChanged)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  mock_source_->StopSource();
  run_loop.Run();
}

// Inserts an opaque frame followed by two transparent frames and expects the
// newly introduced transparent frame to force keyframe output.
TEST_P(VideoTrackRecorderTestMediaVideoEncoderParam,
       ForceKeyframeOnAlphaSwitch) {
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  const scoped_refptr<media::VideoFrame> opaque_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

  InSequence s;
  base::StringPiece first_frame_encoded_alpha;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .Times(1)
      .WillOnce(SaveArg<2>(&first_frame_encoded_alpha));
  Encode(opaque_frame, base::TimeTicks::Now());

  const scoped_refptr<media::VideoFrame> alpha_frame =
      media::VideoFrame::CreateTransparentFrame(frame_size);
  base::StringPiece second_frame_encoded_alpha;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .Times(1)
      .WillOnce(SaveArg<2>(&second_frame_encoded_alpha));
  Encode(alpha_frame, base::TimeTicks::Now());

  base::RunLoop run_loop;
  base::StringPiece third_frame_encoded_alpha;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, false))
      .Times(1)
      .WillOnce(DoAll(SaveArg<2>(&third_frame_encoded_alpha),
                      RunClosure(run_loop.QuitClosure())));
  Encode(alpha_frame, base::TimeTicks::Now());
  run_loop.Run();

  const size_t kEmptySize = 0;
  EXPECT_EQ(first_frame_encoded_alpha.size(), kEmptySize);
  EXPECT_GT(second_frame_encoded_alpha.size(), kEmptySize);
  EXPECT_GT(third_frame_encoded_alpha.size(), kEmptySize);

  Mock::VerifyAndClearExpectations(this);
}

// Inserts an OnError() call between sent frames.
TEST_P(VideoTrackRecorderTestMediaVideoEncoderParam, HandlesOnError) {
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

  InSequence s;
  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .WillOnce(RunClosure(run_loop1.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  run_loop1.Run();

  EXPECT_TRUE(HasEncoderInstance());
  OnError();
  EXPECT_FALSE(HasEncoderInstance());

  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .WillOnce(RunClosure(run_loop2.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  run_loop2.Run();

  Mock::VerifyAndClearExpectations(this);
}

// Hardware encoder fails and fallbacks a software encoder.
TEST_P(VideoTrackRecorderTestMediaVideoEncoderParam,
       HandleSoftwareEncoderFallback) {
  // Skip this test case with VEAEncoder.
  // VEAEncoder drops frames until RequireBitstreamBufferReady() is dropped.
  // It is tricky to pass this test with VEAEncoder due to the issue.
  if (!GetParam()) {
    GTEST_SKIP();
  }
  media::MockGpuVideoAcceleratorFactories mock_gpu_factories(nullptr);
  EXPECT_CALL(*platform_, GetGpuFactories())
      .WillRepeatedly(Return(&mock_gpu_factories));
  EXPECT_CALL(mock_gpu_factories, NotifyEncoderSupportKnown)
      .WillRepeatedly(base::test::RunOnceClosure<0>());
  EXPECT_CALL(mock_gpu_factories, GetTaskRunner)
      .WillRepeatedly(Return(scheduler::GetSingleThreadTaskRunnerForTesting()));
  EXPECT_CALL(mock_gpu_factories, GetVideoEncodeAcceleratorSupportedProfiles)
      .WillRepeatedly(
          Return(std::vector<media::VideoEncodeAccelerator::SupportedProfile>{
              media::VideoEncodeAccelerator::SupportedProfile(
                  media::VideoCodecProfile::VP8PROFILE_ANY,
                  gfx::Size(1920, 1080)),
          }));
  EXPECT_CALL(mock_gpu_factories, DoCreateVideoEncodeAccelerator)
      .WillRepeatedly([]() {
        return new media::FakeVideoEncodeAccelerator(
            scheduler::GetSingleThreadTaskRunnerForTesting());
      });
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size =
      gfx::Size(kVEAEncoderMinResolutionWidth, kVEAEncoderMinResolutionHeight);
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

  InSequence s;
  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo)
      .WillOnce(RunClosure(run_loop1.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  run_loop1.Run();

  EXPECT_TRUE(HasEncoderInstance());
  OnError();
  EXPECT_FALSE(HasEncoderInstance());
  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo)
      .WillOnce(RunClosure(run_loop2.QuitClosure()));
  // Create a software video encoder by setting |allow_vea_encoder| to false.
  Encode(video_frame, base::TimeTicks::Now(), /*allow_vea_encoder=*/false);
  run_loop2.Run();

  Mock::VerifyAndClearExpectations(this);
}

// Inserts a frame for encode and makes sure that it is released.
TEST_P(VideoTrackRecorderTestMediaVideoEncoderParam, ReleasesFrame) {
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

  base::RunLoop run_loop;
  video_frame->AddDestructionObserver(base::BindOnce(run_loop.QuitClosure()));
  Encode(std::move(video_frame), base::TimeTicks::Now());
  run_loop.Run();

  Mock::VerifyAndClearExpectations(this);
}

// Waits for HW encoder support to be enumerated before setting up and
// performing an encode.
TEST_P(VideoTrackRecorderTestMediaVideoEncoderParam, WaitForEncoderSupport) {
  media::MockGpuVideoAcceleratorFactories mock_gpu_factories(nullptr);
  EXPECT_CALL(*platform_, GetGpuFactories())
      .WillRepeatedly(Return(&mock_gpu_factories));

  EXPECT_CALL(mock_gpu_factories, NotifyEncoderSupportKnown)
      .WillOnce(base::test::RunOnceClosure<0>());

#if BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
  media::MockGpuVideoAcceleratorFactories mock_external_software_factories(
      nullptr);
  EXPECT_CALL(*platform_, GetExternalSoftwareFactories())
      .WillRepeatedly(Return(&mock_external_software_factories));
  EXPECT_CALL(mock_external_software_factories, NotifyEncoderSupportKnown(_))
      .WillRepeatedly(base::test::RunOnceClosure<0>());
#endif  // BUILDFLAG(ENABLE_EXTERNAL_OPENH264)

  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_callback_interface_, OnEncodedVideo(_, _, _, _, _, true))
      .WillOnce(RunClosure(run_loop.QuitWhenIdleClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  run_loop.Run();
}

TEST_P(VideoTrackRecorderTestMediaVideoEncoderParam, RequiredRefreshRate) {
  // |RequestRefreshFrame| will be called first by |AddSink| and the second time
  // by the refresh timer using the required min fps.
  EXPECT_CALL(*mock_source_, OnRequestRefreshFrame).Times(2);

  track_->SetIsScreencastForTesting(true);
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  EXPECT_EQ(video_track_recorder_->GetRequiredMinFramesPerSec(), 1);

  test::RunDelayedTasks(base::Seconds(1));
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoTrackRecorderTestMediaVideoEncoderParam,
                         ::testing::Bool());
class VideoTrackRecorderPassthroughTest
    : public TestWithParam<VideoTrackRecorder::CodecId>,
      public VideoTrackRecorderTestBase {
 public:
  using CodecId = VideoTrackRecorder::CodecId;

  VideoTrackRecorderPassthroughTest()
      : mock_source_(new MockMediaStreamVideoSource()) {
    ON_CALL(*mock_source_, SupportsEncodedOutput).WillByDefault(Return(true));
    const String track_id("dummy");
    source_ = MakeGarbageCollected<MediaStreamSource>(
        track_id, MediaStreamSource::kTypeVideo, track_id, false /*remote*/,
        base::WrapUnique(mock_source_.get()));
    component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        source_, std::make_unique<MediaStreamVideoTrack>(
                     mock_source_,
                     WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
                     true /* enabled */));

    // Paranoia checks.
    EXPECT_EQ(component_->Source()->GetPlatformSource(),
              source_->GetPlatformSource());
    EXPECT_TRUE(scheduler::GetSingleThreadTaskRunnerForTesting()
                    ->BelongsToCurrentThread());
  }

  ~VideoTrackRecorderPassthroughTest() override {
    component_ = nullptr;
    source_ = nullptr;
    video_track_recorder_.reset();
    WebHeap::CollectAllGarbageForTesting();
  }

  void InitializeRecorder() {
    video_track_recorder_ = std::make_unique<VideoTrackRecorderPassthrough>(
        scheduler::GetSingleThreadTaskRunnerForTesting(),
        WebMediaStreamTrack(component_.Get()), mock_callback_interface_,
        KeyFrameRequestProcessor::Configuration());
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  // All members are non-const due to the series of initialize() calls needed.
  // |mock_source_| is owned by |source_|.
  raw_ptr<MockMediaStreamVideoSource, DanglingUntriaged> mock_source_;
  Persistent<MediaStreamSource> source_;
  Persistent<MediaStreamComponent> component_;

  std::unique_ptr<VideoTrackRecorderPassthrough> video_track_recorder_;
};

scoped_refptr<FakeEncodedVideoFrame> CreateFrame(
    bool is_key_frame,
    VideoTrackRecorder::CodecId codec) {
  return FakeEncodedVideoFrame::Builder()
      .WithKeyFrame(is_key_frame)
      .WithData("abc")
      .WithCodec(MediaVideoCodecFromCodecId(codec))
      .BuildRefPtr();
}

TEST_F(VideoTrackRecorderPassthroughTest, RequestsAndFinishesEncodedOutput) {
  EXPECT_CALL(*mock_source_, OnEncodedSinkEnabled);
  EXPECT_CALL(*mock_source_, OnEncodedSinkDisabled);
  InitializeRecorder();
}

void DoNothing() {}

// Matcher for checking codec type
MATCHER_P(IsSameCodec, codec, "") {
  return arg.codec == MediaVideoCodecFromCodecId(codec);
}

TEST_P(VideoTrackRecorderPassthroughTest, HandlesFrames) {
  ON_CALL(*mock_source_, OnEncodedSinkEnabled).WillByDefault(DoNothing);
  ON_CALL(*mock_source_, OnEncodedSinkDisabled).WillByDefault(DoNothing);
  InitializeRecorder();

  // Frame 1 (keyframe)
  auto frame = CreateFrame(/*is_key_frame=*/true, GetParam());
  std::string encoded_data;
  EXPECT_CALL(*mock_callback_interface_,
              OnPassthroughVideo(IsSameCodec(GetParam()), _, _, _, true))
      .WillOnce(DoAll(SaveArg<1>(&encoded_data)));
  auto now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
  EXPECT_EQ(encoded_data, "abc");

  // Frame 2 (deltaframe)
  frame = CreateFrame(/*is_key_frame=*/false, GetParam());
  EXPECT_CALL(*mock_callback_interface_,
              OnPassthroughVideo(IsSameCodec(GetParam()), _, _, _, false));
  now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
}

TEST_F(VideoTrackRecorderPassthroughTest, DoesntForwardDeltaFrameFirst) {
  EXPECT_CALL(*mock_source_, OnEncodedSinkEnabled);
  InitializeRecorder();
  Mock::VerifyAndClearExpectations(mock_source_);

  // Frame 1 (deltaframe) - not forwarded
  auto frame = CreateFrame(/*is_key_frame=*/false, CodecId::kVp9);
  EXPECT_CALL(*mock_callback_interface_, OnPassthroughVideo(_, _, _, _, false))
      .Times(0);
  // We already requested a keyframe when starting the recorder, so expect
  // no keyframe request now
  EXPECT_CALL(*mock_source_, OnEncodedSinkEnabled).Times(0);
  EXPECT_CALL(*mock_source_, OnEncodedSinkDisabled).Times(0);
  auto now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(mock_source_);

  // Frame 2 (keyframe)
  frame = CreateFrame(/*is_key_frame=*/true, CodecId::kVp9);
  EXPECT_CALL(*mock_callback_interface_, OnPassthroughVideo(_, _, _, _, true));
  now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
  Mock::VerifyAndClearExpectations(this);

  // Frame 3 (deltaframe) - forwarded
  base::RunLoop run_loop;
  frame = CreateFrame(/*is_key_frame=*/false, CodecId::kVp9);
  EXPECT_CALL(*mock_callback_interface_, OnPassthroughVideo)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
  run_loop.Run();
  EXPECT_CALL(*mock_source_, OnEncodedSinkDisabled);
}

TEST_F(VideoTrackRecorderPassthroughTest, PausesAndResumes) {
  InitializeRecorder();
  // Frame 1 (keyframe)
  auto frame = CreateFrame(/*is_key_frame=*/true, CodecId::kVp9);
  auto now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
  video_track_recorder_->Pause();

  // Expect no frame throughput now.
  frame = CreateFrame(/*is_key_frame=*/false, CodecId::kVp9);
  EXPECT_CALL(*mock_callback_interface_, OnPassthroughVideo).Times(0);
  now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
  Mock::VerifyAndClearExpectations(this);

  // Resume - expect keyframe request
  Mock::VerifyAndClearExpectations(mock_source_);
  // Expect no callback registration, but expect a keyframe.
  EXPECT_CALL(*mock_source_, OnEncodedSinkEnabled).Times(0);
  EXPECT_CALL(*mock_source_, OnEncodedSinkDisabled).Times(0);
  EXPECT_CALL(*mock_source_, OnRequestKeyFrame);
  video_track_recorder_->Resume();
  Mock::VerifyAndClearExpectations(mock_source_);

  // Expect no transfer from deltaframe and transfer of keyframe
  frame = CreateFrame(/*is_key_frame=*/false, CodecId::kVp9);
  EXPECT_CALL(*mock_callback_interface_, OnPassthroughVideo).Times(0);
  now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
  Mock::VerifyAndClearExpectations(this);

  frame = CreateFrame(/*is_key_frame=*/true, CodecId::kVp9);
  EXPECT_CALL(*mock_callback_interface_, OnPassthroughVideo);
  now = base::TimeTicks::Now();
  video_track_recorder_->OnEncodedVideoFrameForTesting(now, frame, now);
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoTrackRecorderPassthroughTest,
                         ValuesIn(kTrackRecorderTestCodec));

class CodecEnumeratorTest : public ::testing::Test {
 public:
  using CodecEnumerator = VideoTrackRecorder::CodecEnumerator;
  using CodecId = VideoTrackRecorder::CodecId;

  CodecEnumeratorTest() = default;

  CodecEnumeratorTest(const CodecEnumeratorTest&) = delete;
  CodecEnumeratorTest& operator=(const CodecEnumeratorTest&) = delete;

  ~CodecEnumeratorTest() override = default;

  media::VideoEncodeAccelerator::SupportedProfiles MakeVp8Profiles() {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    profiles.emplace_back(media::VP8PROFILE_ANY, gfx::Size(1920, 1080), 30, 1);
    return profiles;
  }

  media::VideoEncodeAccelerator::SupportedProfiles MakeVp9Profiles(
      bool vbr_support = false) {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    auto rc_mode =
        media::VideoEncodeAccelerator::SupportedRateControlMode::kConstantMode;
    if (vbr_support) {
      rc_mode |= media::VideoEncodeAccelerator::SupportedRateControlMode::
          kVariableMode;
    }

    profiles.emplace_back(media::VP9PROFILE_PROFILE1, gfx::Size(1920, 1080), 60,
                          1, rc_mode);
    profiles.emplace_back(media::VP9PROFILE_PROFILE2, gfx::Size(1920, 1080), 30,
                          1, rc_mode);
    return profiles;
  }

  media::VideoEncodeAccelerator::SupportedProfiles MakeVp8Vp9Profiles() {
    media::VideoEncodeAccelerator::SupportedProfiles profiles =
        MakeVp8Profiles();
    media::VideoEncodeAccelerator::SupportedProfiles vp9_profiles =
        MakeVp9Profiles();
    profiles.insert(profiles.end(), vp9_profiles.begin(), vp9_profiles.end());
    return profiles;
  }

  media::VideoEncodeAccelerator::SupportedProfiles MakeH264Profiles(
      bool vbr_support = false) {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    auto rc_mode =
        media::VideoEncodeAccelerator::SupportedRateControlMode::kConstantMode;
    if (vbr_support) {
      rc_mode |= media::VideoEncodeAccelerator::SupportedRateControlMode::
          kVariableMode;
    }

    profiles.emplace_back(media::H264PROFILE_BASELINE, gfx::Size(1920, 1080),
                          24, 1, rc_mode);
    profiles.emplace_back(media::H264PROFILE_MAIN, gfx::Size(1920, 1080), 30, 1,
                          rc_mode);
    profiles.emplace_back(media::H264PROFILE_HIGH, gfx::Size(1920, 1080), 60, 1,
                          rc_mode);
    return profiles;
  }
  test::TaskEnvironment task_environment_;
};

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdDefault) {
  // Empty supported profiles.
  MediaTrackContainerType type = GetMediaContainerTypeFromString("");
  const CodecEnumerator emulator(
      (media::VideoEncodeAccelerator::SupportedProfiles()));
  EXPECT_EQ(CodecId::kVp8, emulator.GetPreferredCodecId(type));
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp8) {
  MediaTrackContainerType type = GetMediaContainerTypeFromString("");
  const CodecEnumerator emulator(MakeVp8Profiles());
  EXPECT_EQ(CodecId::kVp8, emulator.GetPreferredCodecId(type));
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp9) {
  MediaTrackContainerType type = GetMediaContainerTypeFromString("");
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(CodecId::kVp9, emulator.GetPreferredCodecId(type));
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp8Vp9) {
  MediaTrackContainerType type = GetMediaContainerTypeFromString("");
  const CodecEnumerator emulator(MakeVp8Vp9Profiles());
  EXPECT_EQ(CodecId::kVp8, emulator.GetPreferredCodecId(type));
}

TEST_F(CodecEnumeratorTest, MakeSupportedProfilesVp9) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  media::VideoEncodeAccelerator::SupportedProfiles profiles =
      emulator.GetSupportedProfiles(CodecId::kVp9);
  EXPECT_EQ(2u, profiles.size());
  EXPECT_EQ(media::VP9PROFILE_PROFILE1, profiles[0].profile);
  EXPECT_EQ(media::VP9PROFILE_PROFILE2, profiles[1].profile);
}

TEST_F(CodecEnumeratorTest, MakeSupportedProfilesNoVp8) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  media::VideoEncodeAccelerator::SupportedProfiles profiles =
      emulator.GetSupportedProfiles(CodecId::kVp8);
  EXPECT_TRUE(profiles.empty());
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileVp9) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(std::make_pair(media::VP9PROFILE_PROFILE1, /*vbr_support=*/false),
            emulator.GetFirstSupportedVideoCodecProfile(CodecId::kVp9));
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileNoVp8) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(
      std::make_pair(media::VIDEO_CODEC_PROFILE_UNKNOWN, /*vbr_support=*/false),
      emulator.GetFirstSupportedVideoCodecProfile(CodecId::kVp8));
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileVp9VBR) {
  const CodecEnumerator emulator(MakeVp9Profiles(/*vbr_support=*/true));
  EXPECT_EQ(std::make_pair(media::VP9PROFILE_PROFILE1, /*vbr_support=*/true),
            emulator.GetFirstSupportedVideoCodecProfile(CodecId::kVp9));
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileNoVp8VBR) {
  const CodecEnumerator emulator(MakeVp9Profiles(/*vbr_support=*/true));
  EXPECT_EQ(
      std::make_pair(media::VIDEO_CODEC_PROFILE_UNKNOWN, /*vbr_support=*/false),
      emulator.GetFirstSupportedVideoCodecProfile(CodecId::kVp8));
}

#if BUILDFLAG(ENABLE_OPENH264) || BUILDFLAG(ENABLE_EXTERNAL_OPENH264) || \
    BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileH264) {
  const CodecEnumerator emulator(MakeH264Profiles());
  EXPECT_EQ(std::make_pair(media::H264PROFILE_HIGH, /*vbr_support=*/false),
            emulator.FindSupportedVideoCodecProfile(CodecId::kH264,
                                                    media::H264PROFILE_HIGH));
}

TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileH264VBR) {
  const CodecEnumerator emulator(MakeH264Profiles(/*vbr_support=*/true));
  EXPECT_EQ(std::make_pair(media::H264PROFILE_HIGH, /*vbr_support=*/true),
            emulator.FindSupportedVideoCodecProfile(CodecId::kH264,
                                                    media::H264PROFILE_HIGH));
}

TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileNoProfileH264) {
  const CodecEnumerator emulator(MakeH264Profiles());
  EXPECT_EQ(
      std::make_pair(media::VIDEO_CODEC_PROFILE_UNKNOWN, /*vbr_support=*/false),
      emulator.FindSupportedVideoCodecProfile(
          CodecId::kH264, media::H264PROFILE_HIGH422PROFILE));
}

TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileNoProfileH264VBR) {
  const CodecEnumerator emulator(MakeH264Profiles(/*vbr_support=*/true));
  EXPECT_EQ(
      std::make_pair(media::VIDEO_CODEC_PROFILE_UNKNOWN, /*vbr_support=*/false),
      emulator.FindSupportedVideoCodecProfile(
          CodecId::kH264, media::H264PROFILE_HIGH422PROFILE));
}

#endif

}  // namespace blink
