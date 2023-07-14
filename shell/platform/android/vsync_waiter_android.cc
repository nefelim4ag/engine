// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/vsync_waiter_android.h"

#include <cmath>
#include <utility>

#include "flutter/common/task_runners.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/fml/size.h"
#include "flutter/fml/trace_event.h"

namespace flutter {

static fml::jni::ScopedJavaGlobalRef<jclass>* g_vsync_waiter_class = nullptr;
static jmethodID g_async_wait_for_vsync_method_ = nullptr;
static std::atomic_uint g_refresh_rate_ = 60;

VsyncWaiterAndroid::VsyncWaiterAndroid(const flutter::TaskRunners& task_runners)
    : VsyncWaiter(task_runners),
      choreographer_type_(AndroidChoreographer::WhichChoreographer()),
      awaiting_(false),
      looping_(false) {}

VsyncWaiterAndroid::~VsyncWaiterAndroid() = default;

// |VsyncWaiter|
void VsyncWaiterAndroid::AwaitVSync() {
  awaiting_ = true;
  if (!looping_) {
    // Choreographer seems not to callback if we are too close to the
    // next frame, but if we don't get that callback, we will miss a frame.
    // Better to just constantly register for callbacks, and only fire callbacks
    // when [awaiting_].
    looping_ = true;
    Loop();
  }
}

void VsyncWaiterAndroid::Loop() {
  auto* weak_this = new std::weak_ptr<VsyncWaiter>(shared_from_this());
  switch (choreographer_type_) {
    case AndroidChoreographer::ChoreographerType::kJava: {
      // TODO(99798): Remove it when we drop support for API level < 29.
      jlong java_baton = reinterpret_cast<jlong>(weak_this);
      task_runners_.GetPlatformTaskRunner()->PostTask([java_baton]() {
        JNIEnv* env = fml::jni::AttachCurrentThread();
        env->CallStaticVoidMethod(g_vsync_waiter_class->obj(),     //
                                  g_async_wait_for_vsync_method_,  //
                                  java_baton                       //
        );
      });
      break;
    }
    case AndroidChoreographer::ChoreographerType::kNDK: {
      fml::TaskRunner::RunNowOrPostTask(
          task_runners_.GetUITaskRunner(), [weak_this]() {
            AndroidChoreographer::PostFrameCallback(&OnVsyncFromNDK, weak_this);
          });
      break;
    }
    case AndroidChoreographer::ChoreographerType::kNDK33: {
      fml::TaskRunner::RunNowOrPostTask(
          task_runners_.GetUITaskRunner(), [weak_this]() {
            AndroidChoreographer::PostFrameCallback33(&OnVsyncFromNDK33,
                                                      weak_this);
          });
      break;
    }
  }
}

// static
void VsyncWaiterAndroid::OnVsyncFromNDK(int64_t frame_nanos, void* data) {
  auto frame_time = fml::TimePoint::FromEpochDelta(
      fml::TimeDelta::FromNanoseconds(frame_nanos));
  auto now = fml::TimePoint::Now();
  if (frame_time > now) {
    frame_time = now;
  }
  auto target_time = frame_time + fml::TimeDelta::FromNanoseconds(
                                      1000000000.0 / g_refresh_rate_);

  TRACE_EVENT2_INT("flutter", "PlatformVsync", "frame_start_time",
                   frame_time.ToEpochDelta().ToMicroseconds(),
                   "frame_target_time",
                   target_time.ToEpochDelta().ToMicroseconds());

  auto* weak_this = reinterpret_cast<std::weak_ptr<VsyncWaiterAndroid>*>(data);
  ConsumePendingCallback(weak_this, frame_time, target_time, target_time, 0);
}

// static
void VsyncWaiterAndroid::OnVsyncFromNDK33(
    AndroidChoreographer::AChoreographerFrameCallbackData* callback_data,
    void* data) {
  auto frame_time = AndroidChoreographer::GetFrameTime(callback_data);
  auto now = fml::TimePoint::Now();
  if (frame_time > now) {
    frame_time = now;
  }
  auto* weak_this = reinterpret_cast<std::weak_ptr<VsyncWaiterAndroid>*>(data);
  auto shared_this = weak_this->lock();
  if (!shared_this) {
    delete weak_this;
    return;
  }
  // Target to present with +1 frame latency to avoid jitter from
  // uneven frame pacing.
  auto target_time = AndroidChoreographer::GetFrameDeadline(callback_data, 1);
  auto next_frame_time = frame_time + fml::TimeDelta::FromNanoseconds(
                                          1000000000.0 / g_refresh_rate_);
  auto vsync_id = AndroidChoreographer::GetFrameVsyncId(callback_data, 1);

  TRACE_EVENT4_INT(
      "flutter", "PlatformVsync", "frame_start_time",
      frame_time.ToEpochDelta().ToMicroseconds(), "frame_target_time",
      target_time.ToEpochDelta().ToMicroseconds(), "next_frame_start_time",
      next_frame_time.ToEpochDelta().ToMicroseconds(), "frame_target_vsync_id",
      vsync_id);

  ConsumePendingCallback(weak_this, frame_time, target_time, next_frame_time,
                         vsync_id);
}

// static
void VsyncWaiterAndroid::OnVsyncFromJava(JNIEnv* env,
                                         jclass jcaller,
                                         jlong frameDelayNanos,
                                         jlong refreshPeriodNanos,
                                         jlong java_baton) {
  auto frame_time =
      fml::TimePoint::Now() - fml::TimeDelta::FromNanoseconds(frameDelayNanos);
  auto target_time =
      frame_time + fml::TimeDelta::FromNanoseconds(refreshPeriodNanos);

  TRACE_EVENT2_INT("flutter", "PlatformVsync", "frame_start_time",
                   frame_time.ToEpochDelta().ToMicroseconds(),
                   "frame_target_time",
                   target_time.ToEpochDelta().ToMicroseconds());

  auto* weak_this =
      reinterpret_cast<std::weak_ptr<VsyncWaiterAndroid>*>(java_baton);
  ConsumePendingCallback(weak_this, frame_time, target_time, target_time, 0);
}

// static
void VsyncWaiterAndroid::ConsumePendingCallback(
    std::weak_ptr<VsyncWaiterAndroid>* weak_this,
    fml::TimePoint frame_start_time,
    fml::TimePoint frame_target_time,
    fml::TimePoint next_frame_start_time,
    int64_t frame_vsync_id) {
  auto shared_this = weak_this->lock();
  delete weak_this;

  if (shared_this) {
    if (shared_this->awaiting_) {
      shared_this->awaiting_ = false;
      shared_this->FireCallback(frame_start_time, frame_target_time,
                                next_frame_start_time, frame_vsync_id);
    }
    shared_this->Loop();
  }
}

// static
void VsyncWaiterAndroid::OnUpdateRefreshRate(JNIEnv* env,
                                             jclass jcaller,
                                             jfloat refresh_rate) {
  FML_DCHECK(refresh_rate > 0);
  g_refresh_rate_ = static_cast<uint>(refresh_rate);
}

// static
bool VsyncWaiterAndroid::Register(JNIEnv* env) {
  static const JNINativeMethod methods[] = {
      {
          .name = "nativeOnVsync",
          .signature = "(JJJ)V",
          .fnPtr = reinterpret_cast<void*>(&OnVsyncFromJava),
      },
      {
          .name = "nativeUpdateRefreshRate",
          .signature = "(F)V",
          .fnPtr = reinterpret_cast<void*>(&OnUpdateRefreshRate),
      }};

  jclass clazz = env->FindClass("io/flutter/embedding/engine/FlutterJNI");

  if (clazz == nullptr) {
    return false;
  }

  g_vsync_waiter_class = new fml::jni::ScopedJavaGlobalRef<jclass>(env, clazz);
  FML_CHECK(!g_vsync_waiter_class->is_null());

  g_async_wait_for_vsync_method_ = env->GetStaticMethodID(
      g_vsync_waiter_class->obj(), "asyncWaitForVsync", "(J)V");
  FML_CHECK(g_async_wait_for_vsync_method_ != nullptr);

  return env->RegisterNatives(clazz, methods, fml::size(methods)) == 0;
}

}  // namespace flutter
