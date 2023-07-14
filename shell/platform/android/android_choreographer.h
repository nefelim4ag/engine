// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_CHOREOGRAPHER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_CHOREOGRAPHER_H_

#include "flutter/fml/macros.h"
#include "flutter/fml/time/time_point.h"

#include <cstdint>

namespace flutter {

//------------------------------------------------------------------------------
/// The Android Choreographer is used by `VsyncWaiterAndroid` to await vsync
/// signal. It's only available on API 29+ or API 24+ if the architecture is
/// 64-bit.
///
class AndroidChoreographer {
 public:
  // Only available on API 33+
  typedef void AChoreographerFrameCallbackData;
  typedef void (*OnFrameCallback)(int64_t frame_time_nanos, void* data);
  typedef void (*OnFrameCallback33)(
      AChoreographerFrameCallbackData* callback_data,
      void* data);

  enum ChoreographerType { kJava, kNDK, kNDK33 };
  static ChoreographerType WhichChoreographer();
  static void PostFrameCallback(OnFrameCallback callback, void* data);
  static void PostFrameCallback33(OnFrameCallback33 callback, void* data);
  static fml::TimePoint GetFrameTime(AChoreographerFrameCallbackData* data);
  static fml::TimePoint GetFrameDeadline(AChoreographerFrameCallbackData* data,
                                         size_t latency);
  static int64_t GetFrameVsyncId(AChoreographerFrameCallbackData* data,
                                 size_t latency);

 private:
  static int64_t GetFrameTimelineIndex(AChoreographerFrameCallbackData* data,
                                       size_t latency);
  FML_DISALLOW_COPY_AND_ASSIGN(AndroidChoreographer);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_CHOREOGRAPHER_H_
