/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/controller/blink_initializer.h"

#include <memory>

#include "build/build_config.h"
#include "third_party/blink/public/common/experiments/memory_ablation_experiment.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_context_snapshot_external_references.h"
#include "third_party/blink/renderer/controller/blink_leak_detector.h"
#include "third_party/blink/renderer/controller/bloated_renderer_detector.h"
#include "third_party/blink/renderer/controller/dev_tools_frontend_impl.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/display_cutout_client_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"
#include "third_party/blink/renderer/controller/oom_intervention_impl.h"
#endif

namespace blink {

namespace {

class EndOfTaskRunner : public Thread::TaskObserver {
 public:
  void WillProcessTask() override { AnimationClock::NotifyTaskStart(); }
  void DidProcessTask() override {
    Microtask::PerformCheckpoint(V8PerIsolateData::MainThreadIsolate());
    V8Initializer::ReportRejectedPromisesOnMainThread();
  }
};

Thread::TaskObserver* g_end_of_task_runner = nullptr;

BlinkInitializer& GetBlinkInitializer() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<BlinkInitializer>, initializer,
                      (std::make_unique<BlinkInitializer>()));
  return *initializer;
}

void InitializeCommon(Platform* platform,
                      service_manager::BinderRegistry* registry) {
#if !defined(ARCH_CPU_X86_64) && !defined(ARCH_CPU_ARM64) && defined(OS_WIN)
  // Reserve address space on 32 bit Windows, to make it likelier that large
  // array buffer allocations succeed.
  BOOL is_wow_64 = -1;
  if (!IsWow64Process(GetCurrentProcess(), &is_wow_64))
    is_wow_64 = FALSE;
  if (!is_wow_64) {
    // Try to reserve as much address space as we reasonably can.
    const size_t kMB = 1024 * 1024;
    for (size_t size = 512 * kMB; size >= 32 * kMB; size -= 16 * kMB) {
      if (base::ReserveAddressSpace(size)) {
        // Report successful reservation.
        DEFINE_STATIC_LOCAL(CustomCountHistogram, reservation_size_histogram,
                            ("Renderer4.ReservedMemory", 32, 512, 32));
        reservation_size_histogram.Count(size / kMB);

        break;
      }
    }
  }
#endif  // !defined(ARCH_CPU_X86_64) && !defined(ARCH_CPU_ARM64) &&
        // defined(OS_WIN)

  // BlinkInitializer::Initialize() must be called before InitializeMainThread
  GetBlinkInitializer().Initialize();

  if (RuntimeEnabledFeatures::BloatedRendererDetectionEnabled()) {
    BloatedRendererDetector::Initialize();
    V8Initializer::SetNearV8HeapLimitOnMainThreadCallback(
        BloatedRendererDetector::OnNearV8HeapLimitOnMainThread);
  }

  V8Initializer::InitializeMainThread(
      V8ContextSnapshotExternalReferences::GetTable());

  GetBlinkInitializer().RegisterInterfaces(*registry);

  // currentThread is null if we are running on a thread without a message loop.
  if (Thread* current_thread = platform->CurrentThread()) {
    DCHECK(!g_end_of_task_runner);
    g_end_of_task_runner = new EndOfTaskRunner;
    current_thread->AddTaskObserver(g_end_of_task_runner);
  }

  if (Thread* main_thread = Platform::Current()->MainThread()) {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        main_thread->GetTaskRunner();
    if (task_runner)
      MemoryAblationExperiment::MaybeStartForRenderer(task_runner);
  }

#if defined(OS_ANDROID)
  // Initialize CrashMemoryMetricsReporterImpl in order to assure that memory
  // allocation does not happen in OnOOMCallback.
  CrashMemoryMetricsReporterImpl::Instance();
#endif
}

}  // namespace

void Initialize(Platform* platform,
                service_manager::BinderRegistry* registry,
                scheduler::WebThreadScheduler* main_thread_scheduler) {
  DCHECK(registry);
  Platform::Initialize(platform, main_thread_scheduler);
  InitializeCommon(platform, registry);
}

void CreateMainThreadAndInitialize(Platform* platform,
                                   service_manager::BinderRegistry* registry) {
  DCHECK(registry);
  Platform::CreateMainThreadAndInitialize(platform);
  InitializeCommon(platform, registry);
}

void BlinkInitializer::RegisterInterfaces(
    service_manager::BinderRegistry& registry) {
  ModulesInitializer::RegisterInterfaces(registry);
  Thread* main_thread = Platform::Current()->MainThread();
  // GetSingleThreadTaskRunner() uses GetTaskRunner() internally.
  // crbug.com/781664
  if (!main_thread || !main_thread->GetTaskRunner())
    return;

#if defined(OS_ANDROID)
  registry.AddInterface(
      ConvertToBaseCallback(CrossThreadBind(&OomInterventionImpl::Create)),
      main_thread->GetTaskRunner());

  registry.AddInterface(ConvertToBaseCallback(CrossThreadBind(
                            &CrashMemoryMetricsReporterImpl::Bind)),
                        main_thread->GetTaskRunner());
#endif

  registry.AddInterface(
      ConvertToBaseCallback(CrossThreadBind(&BlinkLeakDetector::Create)),
      main_thread->GetTaskRunner());
}

void BlinkInitializer::InitLocalFrame(LocalFrame& frame) const {
  if (RuntimeEnabledFeatures::DisplayCutoutAPIEnabled()) {
    frame.GetInterfaceRegistry()->AddAssociatedInterface(WTF::BindRepeating(
        &DisplayCutoutClientImpl::BindMojoRequest, WrapWeakPersistent(&frame)));
  }
  frame.GetInterfaceRegistry()->AddAssociatedInterface(WTF::BindRepeating(
      &DevToolsFrontendImpl::BindMojoRequest, WrapWeakPersistent(&frame)));
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &LocalFrame::PauseSubresourceLoading, WrapWeakPersistent(&frame)));
  frame.GetInterfaceRegistry()->AddInterface(
      WTF::BindRepeating(&LocalFrame::BindPreviewsResourceLoadingHintsRequest,
                         WrapWeakPersistent(&frame)));
  ModulesInitializer::InitLocalFrame(frame);
}

void BlinkInitializer::OnClearWindowObjectInMainWorld(
    Document& document,
    const Settings& settings) const {
  if (DevToolsFrontendImpl* devtools_frontend =
          DevToolsFrontendImpl::From(document.GetFrame())) {
    devtools_frontend->DidClearWindowObject();
  }
  ModulesInitializer::OnClearWindowObjectInMainWorld(document, settings);
}

}  // namespace blink
