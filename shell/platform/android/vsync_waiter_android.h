// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_PLATFORM_ANDROID_VSYNC_WAITER_ANDROID_H_
#define SHELL_PLATFORM_ANDROID_VSYNC_WAITER_ANDROID_H_

#include <jni.h>

#include <memory>

#include "flutter/fml/macros.h"
#include "flutter/shell/common/vsync_waiter.h"
#include "flutter/shell/platform/android/android_choreographer.h"

namespace flutter {

class VsyncWaiterAndroid final : public VsyncWaiter {
 public:
  static bool Register(JNIEnv* env);

  explicit VsyncWaiterAndroid(const flutter::TaskRunners& task_runners);

  ~VsyncWaiterAndroid() override;

 private:
  // |VsyncWaiter|
  void AwaitVSync() override;

  void Loop();

  static void OnVsyncFromNDK(int64_t vsync_nanos, void* data);

  static void OnVsyncFromNDK33(
      AndroidChoreographer::AChoreographerFrameCallbackData* callback_data,
      void* data);

  static void OnVsyncFromJava(JNIEnv* env,
                              jclass jcaller,
                              jlong frameDelayNanos,
                              jlong refreshPeriodNanos,
                              jlong java_baton);

  static void ConsumePendingCallback(
      std::weak_ptr<VsyncWaiterAndroid>* weak_this,
      fml::TimePoint frame_start_time,
      fml::TimePoint frame_target_time,
      fml::TimePoint next_frame_start_time,
      int64_t frame_vsync_id);

  static void OnUpdateRefreshRate(JNIEnv* env,
                                  jclass jcaller,
                                  jfloat refresh_rate);

  const AndroidChoreographer::ChoreographerType choreographer_type_;
  bool awaiting_;
  bool looping_;
  FML_DISALLOW_COPY_AND_ASSIGN(VsyncWaiterAndroid);
};

}  // namespace flutter

#endif  // SHELL_PLATFORM_ANDROID_ASYNC_WAITER_ANDROID_H_
