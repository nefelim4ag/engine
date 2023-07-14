// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/surface/android_surface_transaction.h"

#include "flutter/fml/native_library.h"

namespace flutter {

AndroidSurfaceTransaction::AndroidSurfaceTransaction()
    : pending_transaction_(nullptr) {
#if FML_OS_ANDROID
  auto libandroid = fml::NativeLibrary::Create("libandroid.so");
  FML_DCHECK(libandroid);
  ASurfaceTransaction_create =
      libandroid
          ->ResolveFunction<ASurfaceTransaction_create_FPN>(
              "ASurfaceTransaction_create")
          .value_or(nullptr);
  ASurfaceTransaction_setFrameTimeline =
      libandroid
          ->ResolveFunction<ASurfaceTransaction_setFrameTimeline_FPN>(
              "ASurfaceTransaction_setFrameTimeline")
          .value_or(nullptr);
  ASurfaceTransaction_apply =
      libandroid
          ->ResolveFunction<ASurfaceTransaction_apply_FPN>(
              "ASurfaceTransaction_apply")
          .value_or(nullptr);
  ASurfaceTransaction_delete =
      libandroid
          ->ResolveFunction<ASurfaceTransaction_delete_FPN>(
              "ASurfaceTransaction_delete")
          .value_or(nullptr);
  if (!ASurfaceTransaction_create || !ASurfaceTransaction_setFrameTimeline ||
      !ASurfaceTransaction_apply || !ASurfaceTransaction_delete) {
    ASurfaceTransaction_create = nullptr;
    ASurfaceTransaction_setFrameTimeline = nullptr;
    ASurfaceTransaction_apply = nullptr;
    ASurfaceTransaction_delete = nullptr;
  }
#else
  ASurfaceTransaction_create = nullptr;
  ASurfaceTransaction_setFrameTimeline = nullptr;
  ASurfaceTransaction_apply = nullptr;
  ASurfaceTransaction_delete = nullptr;
#endif  // FML_OS_ANDROID
}

void AndroidSurfaceTransaction::Begin() {
  FML_DCHECK(!pending_transaction_);
  if (ASurfaceTransaction_create) {
    pending_transaction_ = ASurfaceTransaction_create();
  }
}

void AndroidSurfaceTransaction::SetVsyncId(int64_t vsync_id) {
  if (ASurfaceTransaction_create) {
    FML_DCHECK(pending_transaction_);
    if (vsync_id != 0) {
      ASurfaceTransaction_setFrameTimeline(pending_transaction_, vsync_id);
    }
  }
}

void AndroidSurfaceTransaction::End() {
  if (ASurfaceTransaction_create) {
    FML_DCHECK(pending_transaction_);
    ASurfaceTransaction_apply(pending_transaction_);
    ASurfaceTransaction_delete(pending_transaction_);
    pending_transaction_ = nullptr;
  }
}

}  // namespace flutter
