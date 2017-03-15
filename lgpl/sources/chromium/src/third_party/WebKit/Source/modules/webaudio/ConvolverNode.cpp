/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/ConvolverNode.h"
#include "modules/webaudio/ConvolverOptions.h"
#include "platform/audio/Reverb.h"
#include "wtf/PtrUtil.h"
#include <memory>

// Note about empirical tuning:
// The maximum FFT size affects reverb performance and accuracy.
// If the reverb is single-threaded and processes entirely in the real-time
// audio thread, it's important not to make this too high.  In this case 8192 is
// a good value.  But, the Reverb object is multi-threaded, so we want this as
// high as possible without losing too much accuracy.  Very large FFTs will have
// worse phase errors. Given these constraints 32768 is a good compromise.
const size_t MaxFFTSize = 32768;

namespace blink {

ConvolverHandler::ConvolverHandler(AudioNode& node, float sampleRate)
    : AudioHandler(NodeTypeConvolver, node, sampleRate), m_normalize(true) {
  addInput();
  addOutput(2);

  // Node-specific default mixing rules.
  m_channelCount = 2;
  setInternalChannelCountMode(ClampedMax);
  setInternalChannelInterpretation(AudioBus::Speakers);

  initialize();
}

PassRefPtr<ConvolverHandler> ConvolverHandler::create(AudioNode& node,
                                                      float sampleRate) {
  return adoptRef(new ConvolverHandler(node, sampleRate));
}

ConvolverHandler::~ConvolverHandler() {
  uninitialize();
}

void ConvolverHandler::process(size_t framesToProcess) {
  AudioBus* outputBus = output(0).bus();
  DCHECK(outputBus);

  // Synchronize with possible dynamic changes to the impulse response.
  MutexTryLocker tryLocker(m_processLock);
  if (tryLocker.locked()) {
    if (!isInitialized() || !m_reverb) {
      outputBus->zero();
    } else {
      // Process using the convolution engine.
      // Note that we can handle the case where nothing is connected to the
      // input, in which case we'll just feed silence into the convolver.
      // FIXME:  If we wanted to get fancy we could try to factor in the 'tail
      // time' and stop processing once the tail dies down if
      // we keep getting fed silence.
      m_reverb->process(input(0).bus(), outputBus, framesToProcess);
    }
  } else {
    // Too bad - the tryLock() failed.  We must be in the middle of setting a
    // new impulse response.
    outputBus->zero();
  }
}

void ConvolverHandler::setBuffer(AudioBuffer* buffer,
                                 ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (!buffer)
    return;

  if (buffer->sampleRate() != context()->sampleRate()) {
    exceptionState.throwDOMException(
        NotSupportedError,
        "The buffer sample rate of " + String::number(buffer->sampleRate()) +
            " does not match the context rate of " +
            String::number(context()->sampleRate()) + " Hz.");
    return;
  }

  unsigned numberOfChannels = buffer->numberOfChannels();
  size_t bufferLength = buffer->length();

  // The current implementation supports only 1-, 2-, or 4-channel impulse
  // responses, with the 4-channel response being interpreted as true-stereo
  // (see Reverb class).
  bool isChannelCountGood =
      numberOfChannels == 1 || numberOfChannels == 2 || numberOfChannels == 4;

  if (!isChannelCountGood) {
    exceptionState.throwDOMException(
        NotSupportedError, "The buffer must have 1, 2, or 4 channels, not " +
                               String::number(numberOfChannels));
    return;
  }

  // Wrap the AudioBuffer by an AudioBus. It's an efficient pointer set and not
  // a memcpy().  This memory is simply used in the Reverb constructor and no
  // reference to it is kept for later use in that class.
  RefPtr<AudioBus> bufferBus =
      AudioBus::create(numberOfChannels, bufferLength, false);
  for (unsigned i = 0; i < numberOfChannels; ++i)
    bufferBus->setChannelMemory(i, buffer->getChannelData(i)->data(),
                                bufferLength);

  bufferBus->setSampleRate(buffer->sampleRate());

  // Create the reverb with the given impulse response.
  std::unique_ptr<Reverb> reverb = wrapUnique(new Reverb(
      bufferBus.get(), AudioUtilities::kRenderQuantumFrames, MaxFFTSize, 2,
      context() && context()->hasRealtimeConstraint(), m_normalize));

  {
    // Synchronize with process().
    MutexLocker locker(m_processLock);
    m_reverb = std::move(reverb);
    m_buffer = buffer;
  }
}

AudioBuffer* ConvolverHandler::buffer() {
  DCHECK(isMainThread());
  return m_buffer.get();
}

double ConvolverHandler::tailTime() const {
  MutexTryLocker tryLocker(m_processLock);
  if (tryLocker.locked())
    return m_reverb
               ? m_reverb->impulseResponseLength() /
                     static_cast<double>(sampleRate())
               : 0;
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

double ConvolverHandler::latencyTime() const {
  MutexTryLocker tryLocker(m_processLock);
  if (tryLocker.locked())
    return m_reverb
               ? m_reverb->latencyFrames() / static_cast<double>(sampleRate())
               : 0;
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

// ----------------------------------------------------------------

ConvolverNode::ConvolverNode(BaseAudioContext& context) : AudioNode(context) {
  setHandler(ConvolverHandler::create(*this, context.sampleRate()));
}

ConvolverNode* ConvolverNode::create(BaseAudioContext& context,
                                     ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (context.isContextClosed()) {
    context.throwExceptionForClosedState(exceptionState);
    return nullptr;
  }

  return new ConvolverNode(context);
}

ConvolverNode* ConvolverNode::create(BaseAudioContext* context,
                                     const ConvolverOptions& options,
                                     ExceptionState& exceptionState) {
  ConvolverNode* node = create(*context, exceptionState);

  if (!node)
    return nullptr;

  node->handleChannelOptions(options, exceptionState);

  // It is important to set normalize first because setting the buffer will
  // examing the normalize attribute to see if normalization needs to be done.
  node->setNormalize(!options.disableNormalization());
  if (options.hasBuffer())
    node->setBuffer(options.buffer(), exceptionState);
  return node;
}

ConvolverHandler& ConvolverNode::convolverHandler() const {
  return static_cast<ConvolverHandler&>(handler());
}

AudioBuffer* ConvolverNode::buffer() const {
  return convolverHandler().buffer();
}

void ConvolverNode::setBuffer(AudioBuffer* newBuffer,
                              ExceptionState& exceptionState) {
  convolverHandler().setBuffer(newBuffer, exceptionState);
}

bool ConvolverNode::normalize() const {
  return convolverHandler().normalize();
}

void ConvolverNode::setNormalize(bool normalize) {
  convolverHandler().setNormalize(normalize);
}

}  // namespace blink
