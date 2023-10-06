// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_metadata.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface>
        webrtc_audio_frame)
    : delegate_(base::MakeRefCounted<RTCEncodedAudioFrameDelegate>(
          std::move(webrtc_audio_frame),
          webrtc_audio_frame ? webrtc_audio_frame->GetContributingSources()
                             : Vector<uint32_t>(),
          webrtc_audio_frame ? webrtc_audio_frame->SequenceNumber()
                             : absl::nullopt)) {}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    scoped_refptr<RTCEncodedAudioFrameDelegate> delegate)
    : RTCEncodedAudioFrame(delegate->CloneWebRtcFrame()) {}

uint32_t RTCEncodedAudioFrame::timestamp() const {
  return delegate_->Timestamp();
}

DOMArrayBuffer* RTCEncodedAudioFrame::data() const {
  if (!frame_data_) {
    frame_data_ = delegate_->CreateDataBuffer();
  }
  return frame_data_;
}

RTCEncodedAudioFrameMetadata* RTCEncodedAudioFrame::getMetadata() const {
  RTCEncodedAudioFrameMetadata* metadata =
      RTCEncodedAudioFrameMetadata::Create();
  if (delegate_->Ssrc()) {
    metadata->setSynchronizationSource(*delegate_->Ssrc());
  }
  metadata->setContributingSources(delegate_->ContributingSources());
  if (delegate_->PayloadType()) {
    metadata->setPayloadType(*delegate_->PayloadType());
  }
  if (delegate_->SequenceNumber()) {
    metadata->setSequenceNumber(*delegate_->SequenceNumber());
  }
  if (delegate_->AbsCaptureTime()) {
    metadata->setAbsCaptureTime(*delegate_->AbsCaptureTime());
  }
  return metadata;
}

void RTCEncodedAudioFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

void RTCEncodedAudioFrame::setTimestamp(uint32_t timestamp,
                                        ExceptionState& exception_state) {
  delegate_->SetTimestamp(timestamp, exception_state);
}

String RTCEncodedAudioFrame::toString() const {
  StringBuilder sb;
  sb.Append("RTCEncodedAudioFrame{rtpTimestamp: ");
  sb.AppendNumber(delegate_->Timestamp());
  sb.Append(", size: ");
  sb.AppendNumber(data() ? data()->ByteLength() : 0);
  sb.Append("}");
  return sb.ToString();
}

RTCEncodedAudioFrame* RTCEncodedAudioFrame::clone(
    ExceptionState& exception_state) const {
  std::unique_ptr<webrtc::TransformableAudioFrameInterface> new_webrtc_frame =
      delegate_->CloneWebRtcFrame();
  // Clone should never fail.
  CHECK(new_webrtc_frame);
  return MakeGarbageCollected<RTCEncodedAudioFrame>(
      std::move(new_webrtc_frame));
}

void RTCEncodedAudioFrame::SyncDelegate() const {
  delegate_->SetData(frame_data_);
}

scoped_refptr<RTCEncodedAudioFrameDelegate> RTCEncodedAudioFrame::Delegate()
    const {
  SyncDelegate();
  return delegate_;
}

std::unique_ptr<webrtc::TransformableAudioFrameInterface>
RTCEncodedAudioFrame::PassWebRtcFrame() {
  SyncDelegate();
  return delegate_->PassWebRtcFrame();
}

void RTCEncodedAudioFrame::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
}

}  // namespace blink
