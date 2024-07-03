// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_MESH_2D_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_MESH_2D_BUFFER_H_

#include "base/memory/scoped_refptr.h"
#include "cc/paint/refcounted_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// JS wrapper for a retained mesh buffer.
//
// This is the base for vertex/uv/index JS buffers (SkPoint/SkPoint/uint16_t
// specializations, respectively).
//
// The actual data payload is stored in a RefcountedBuffer, which enables
// sharing with the rest of paint pipeline, and avoids deep copies during
// paint op recording.
template <typename T>
class Mesh2DBuffer : public ScriptWrappable {
 public:
  Mesh2DBuffer(const Mesh2DBuffer&) = delete;
  Mesh2DBuffer& operator=(const Mesh2DBuffer&) = delete;

  ~Mesh2DBuffer() override {
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -base::checked_cast<int64_t>(buffer_->data().size() * sizeof(T)));
  }

  scoped_refptr<cc::RefCountedBuffer<T>> GetBuffer() const { return buffer_; }

 protected:
  explicit Mesh2DBuffer(scoped_refptr<cc::RefCountedBuffer<T>> buffer)
      : buffer_(std::move(buffer)) {
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        base::checked_cast<int64_t>(buffer_->data().size() * sizeof(T)));
  }

 private:
  scoped_refptr<cc::RefCountedBuffer<T>> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_MESH_2D_BUFFER_H_
