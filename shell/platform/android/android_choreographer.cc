// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_choreographer.h"

#include "flutter/fml/native_library.h"

// Only avialalbe on API 24+
typedef void AChoreographer;
// Only available on API 29+ or API 24+ if the architecture is 64-bit.
typedef void (*AChoreographer_frameCallback)(int64_t frameTimeNanos,
                                             void* data);
// Only available on API 33+
typedef void (*AChoreographer_vsyncCallback)(
    flutter::AndroidChoreographer::AChoreographerFrameCallbackData*
        callbackData,
    void* data);
// Only avialalbe on API 24+
typedef AChoreographer* (*AChoreographer_getInstance_FPN)();
typedef void (*AChoreographer_postFrameCallback_FPN)(
    AChoreographer* choreographer,
    AChoreographer_frameCallback callback,
    void* data);
// Only available on API 33+
typedef void (*AChoreographer_postVsyncCallback_FPN)(
    AChoreographer* choreographer,
    AChoreographer_vsyncCallback callback,
    void* data);
typedef int64_t (*AChoreographerFrameCallbackData_getFrameTimeNanos_FPN)(
    const flutter::AndroidChoreographer::AChoreographerFrameCallbackData* data);
typedef size_t (
    *AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex_FPN)(
    const flutter::AndroidChoreographer::AChoreographerFrameCallbackData* data);
typedef size_t (*AChoreographerFrameCallbackData_getFrameTimelinesLength_FPN)(
    const flutter::AndroidChoreographer::AChoreographerFrameCallbackData* data);
typedef int64_t (
    *AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos_FPN)(
    const flutter::AndroidChoreographer::AChoreographerFrameCallbackData* data,
    size_t index);
typedef int64_t (*AChoreographerFrameCallbackData_getFrameTimelineVsyncId_FPN)(
    const flutter::AndroidChoreographer::AChoreographerFrameCallbackData* data,
    size_t index);
static AChoreographer_getInstance_FPN AChoreographer_getInstance;
static AChoreographer_postFrameCallback_FPN AChoreographer_postFrameCallback;
static AChoreographer_postVsyncCallback_FPN AChoreographer_postVsyncCallback;
static AChoreographerFrameCallbackData_getFrameTimeNanos_FPN
    AChoreographerFrameCallbackData_getFrameTimeNanos;
static AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex_FPN
    AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex;
static AChoreographerFrameCallbackData_getFrameTimelinesLength_FPN
    AChoreographerFrameCallbackData_getFrameTimelinesLength;
static AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos_FPN
    AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos;
static AChoreographerFrameCallbackData_getFrameTimelineVsyncId_FPN
    AChoreographerFrameCallbackData_getFrameTimelineVsyncId;

namespace flutter {

AndroidChoreographer::ChoreographerType
AndroidChoreographer::WhichChoreographer() {
  static std::optional<AndroidChoreographer::ChoreographerType>
      which_choreographer;
  if (which_choreographer) {
    return which_choreographer.value();
  }
  auto libandroid = fml::NativeLibrary::Create("libandroid.so");
  FML_DCHECK(libandroid);
  auto get_instance_fn =
      libandroid->ResolveFunction<AChoreographer_getInstance_FPN>(
          "AChoreographer_getInstance");
  auto post_frame_callback_fn =
      libandroid->ResolveFunction<AChoreographer_postFrameCallback_FPN>(
          "AChoreographer_postFrameCallback64");
#if FML_ARCH_CPU_64_BITS
  if (!post_frame_callback_fn) {
    post_frame_callback_fn =
        libandroid->ResolveFunction<AChoreographer_postFrameCallback_FPN>(
            "AChoreographer_postFrameCallback");
  }
#endif
  auto post_vsync_callback_fn =
      libandroid->ResolveFunction<AChoreographer_postVsyncCallback_FPN>(
          "AChoreographer_postVsyncCallback");
  auto get_frame_time_nanos_fn = libandroid->ResolveFunction<
      AChoreographerFrameCallbackData_getFrameTimeNanos_FPN>(
      "AChoreographerFrameCallbackData_getFrameTimeNanos");
  auto get_preferred_frame_timeline_index_fn = libandroid->ResolveFunction<
      AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex_FPN>(
      "AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex");
  auto get_frame_timelines_length_fn = libandroid->ResolveFunction<
      AChoreographerFrameCallbackData_getFrameTimelinesLength_FPN>(
      "AChoreographerFrameCallbackData_getFrameTimelinesLength");
  auto get_frame_timeline_deadline_nanos_fn = libandroid->ResolveFunction<
      AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos_FPN>(
      "AChoreographerFrameCallbackData_"
      "getFrameTimelineDeadlineNanos");
  auto get_frame_timeline_vsync_id_fn = libandroid->ResolveFunction<
      AChoreographerFrameCallbackData_getFrameTimelineVsyncId_FPN>(
      "AChoreographerFrameCallbackData_getFrameTimelineVsyncId");
  if (get_instance_fn && post_vsync_callback_fn && get_frame_time_nanos_fn &&
      get_preferred_frame_timeline_index_fn && get_frame_timelines_length_fn &&
      get_frame_timeline_deadline_nanos_fn && get_frame_timeline_vsync_id_fn) {
    AChoreographer_getInstance = get_instance_fn.value();
    AChoreographer_postVsyncCallback = post_vsync_callback_fn.value();
    AChoreographerFrameCallbackData_getFrameTimeNanos =
        get_frame_time_nanos_fn.value();
    AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex =
        get_preferred_frame_timeline_index_fn.value();
    AChoreographerFrameCallbackData_getFrameTimelinesLength =
        get_frame_timelines_length_fn.value();
    AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos =
        get_frame_timeline_deadline_nanos_fn.value();
    AChoreographerFrameCallbackData_getFrameTimelineVsyncId =
        get_frame_timeline_vsync_id_fn.value();
    which_choreographer = AndroidChoreographer::ChoreographerType::kNDK33;
  } else if (get_instance_fn && post_frame_callback_fn) {
    AChoreographer_getInstance = get_instance_fn.value();
    AChoreographer_postFrameCallback = post_frame_callback_fn.value();
    which_choreographer = AndroidChoreographer::ChoreographerType::kNDK;
  } else {
    which_choreographer = AndroidChoreographer::ChoreographerType::kJava;
  }
  return which_choreographer.value();
}

void AndroidChoreographer::PostFrameCallback(OnFrameCallback callback,
                                             void* data) {
  AChoreographer* choreographer = AChoreographer_getInstance();
  AChoreographer_postFrameCallback(choreographer, callback, data);
}

void AndroidChoreographer::PostFrameCallback33(OnFrameCallback33 callback,
                                               void* data) {
  AChoreographer* choreographer = AChoreographer_getInstance();
  AChoreographer_postVsyncCallback(choreographer, callback, data);
}

int64_t AndroidChoreographer::GetFrameTimelineIndex(
    AChoreographerFrameCallbackData* data,
    size_t latency) {
  size_t preferred_index =
      AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex(data) +
      latency;
  return std::min(
      preferred_index,
      AChoreographerFrameCallbackData_getFrameTimelinesLength(data) - 1);
}

fml::TimePoint AndroidChoreographer::GetFrameTime(
    AChoreographerFrameCallbackData* data) {
  return fml::TimePoint::FromEpochDelta(fml::TimeDelta::FromNanoseconds(
      AChoreographerFrameCallbackData_getFrameTimeNanos(data)));
}
fml::TimePoint AndroidChoreographer::GetFrameDeadline(
    AChoreographerFrameCallbackData* data,
    size_t latency) {
  return fml::TimePoint::FromEpochDelta(fml::TimeDelta::FromNanoseconds(
      AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos(
          data, GetFrameTimelineIndex(data, latency))));
}
int64_t AndroidChoreographer::GetFrameVsyncId(
    AChoreographerFrameCallbackData* data,
    size_t latency) {
  return AChoreographerFrameCallbackData_getFrameTimelineVsyncId(
      data, GetFrameTimelineIndex(data, latency));
}

}  // namespace flutter
