// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ============================================================================
// Platform.h
// Top-level platform detection and cross-platform utility includes.
//
// Include this header (directly or transitively) in any file that needs to
// diverge between Windows and non-Windows builds.  All platform-specific
// headers in this directory guard their content with the same macros.
// ============================================================================

#ifndef ORBITER_PLATFORM_H
#define ORBITER_PLATFORM_H

// ---------------------------------------------------------------------------
// Platform detection
// ---------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
#   define ORBITER_WINDOWS 1
#else
#   define ORBITER_WINDOWS 0
#endif

#if defined(__APPLE__)
#   define ORBITER_MACOS 1
#else
#   define ORBITER_MACOS 0
#endif

#if defined(__linux__)
#   define ORBITER_LINUX 1
#else
#   define ORBITER_LINUX 0
#endif

// ---------------------------------------------------------------------------
// On non-Windows, pull in the compatibility shims so that code written
// against Windows APIs can be compiled with minimal changes.
// ---------------------------------------------------------------------------

#if !ORBITER_WINDOWS
#   include "WinTypes.h"
#   include "D3DTypes.h"
#endif

#endif // ORBITER_PLATFORM_H
