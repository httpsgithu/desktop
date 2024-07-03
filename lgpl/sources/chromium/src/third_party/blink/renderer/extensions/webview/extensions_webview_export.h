// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header defines macros to export component's symbols.
// See "platform/platform_export.h" for details.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_WEBVIEW_EXTENSIONS_WEBVIEW_EXPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_WEBVIEW_EXTENSIONS_WEBVIEW_EXPORT_H_

#include "build/build_config.h"

//
// BLINK_EXTENSIONS_WEBVIEW_IMPLEMENTATION
//
#if !defined(BLINK_EXTENSIONS_WEBVIEW_IMPLEMENTATION)
#define BLINK_EXTENSIONS_WEBVIEW_IMPLEMENTATION 0
#endif

//
// EXTENSIONS_WEBVIEW_EXPORT
//
#if !defined(COMPONENT_BUILD)
#define EXTENSIONS_WEBVIEW_EXPORT  // No need of export
#else

#if defined(COMPILER_MSVC)
#if BLINK_EXTENSIONS_WEBVIEW_IMPLEMENTATION
#define EXTENSIONS_WEBVIEW_EXPORT __declspec(dllexport)
#else
#define EXTENSIONS_WEBVIEW_EXPORT __declspec(dllimport)
#endif
#endif  // defined(COMPILER_MSVC)

#if defined(COMPILER_GCC)
#if BLINK_EXTENSIONS_WEBVIEW_IMPLEMENTATION
#define EXTENSIONS_WEBVIEW_EXPORT __attribute__((visibility("default")))
#else
#define EXTENSIONS_WEBVIEW_EXPORT
#endif
#endif  // defined(COMPILER_GCC)

#endif  // !defined(COMPONENT_BUILD)

//
// EXTENSIONS_WEBVIEW_EXTERN_TEMPLATE_EXPORT
// EXTENSIONS_WEBVIEW_TEMPLATE_EXPORT
//
#if BLINK_EXTENSIONS_WEBVIEW_IMPLEMENTATION

#if defined(COMPILER_MSVC)
#define EXTENSIONS_WEBVIEW_EXTERN_TEMPLATE_EXPORT
#define EXTENSIONS_WEBVIEW_TEMPLATE_EXPORT EXTENSIONS_WEBVIEW_EXPORT
#endif

#if defined(COMPILER_GCC)
#define EXTENSIONS_WEBVIEW_EXTERN_TEMPLATE_EXPORT EXTENSIONS_WEBVIEW_EXPORT
#define EXTENSIONS_WEBVIEW_TEMPLATE_EXPORT
#endif

#else  // BLINK_EXTENSIONS_WEBVIEW_IMPLEMENTATION

#define EXTENSIONS_WEBVIEW_EXTERN_TEMPLATE_EXPORT EXTENSIONS_WEBVIEW_EXPORT
#define EXTENSIONS_WEBVIEW_TEMPLATE_EXPORT

#endif  // BLINK_EXTENSIONS_WEBVIEW_IMPLEMENTATION

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_WEBVIEW_EXTENSIONS_WEBVIEW_EXPORT_H_
