// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
#define CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_

#include <jni.h>

#include "base/basictypes.h"

namespace content {

// Registers the callbacks that allows the entry point of the library to be
// exposed to the calling java code.  This handles only registering the content
// specific callbacks.  Any application specific JNI bindings should happen
// once the native library has fully loaded.
bool RegisterLibraryLoaderEntryHook(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_

