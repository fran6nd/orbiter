// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ============================================================================
// WinTypes.h
// Cross-platform replacements for the fundamental Windows types and macros
// used throughout the Orbiter core (DWORD, HRESULT, BOOL, ZeroMemory, …).
//
// Only included on non-Windows platforms via Platform.h.
// On Windows the real <windows.h> is used and this file is never seen.
// ============================================================================

#ifndef ORBITER_WINTYPES_H
#define ORBITER_WINTYPES_H

#if defined(_WIN32) || defined(_WIN64)
#   error "WinTypes.h must not be included on Windows — include <windows.h> instead."
#endif

#include <cstdint>
#include <cstring>   // memset
#include <cstdio>
#include <climits>

// ---------------------------------------------------------------------------
// Scalar types
// ---------------------------------------------------------------------------

typedef unsigned long       DWORD;
typedef unsigned short      WORD;
typedef unsigned char       BYTE;
typedef int                 BOOL;
typedef float               FLOAT;
typedef long                LONG;
typedef unsigned int        UINT;
typedef int                 INT;
typedef char                CHAR;
typedef unsigned char       UCHAR;
typedef void*               LPVOID;
typedef const void*         LPCVOID;
typedef char*               LPSTR;
typedef const char*         LPCSTR;
typedef DWORD*              LPDWORD;

// ---------------------------------------------------------------------------
// HRESULT and standard result codes
// ---------------------------------------------------------------------------

typedef long HRESULT;

#define S_OK            ((HRESULT)0L)
#define S_FALSE         ((HRESULT)1L)
#define E_FAIL          ((HRESULT)0x80004005L)
#define E_INVALIDARG    ((HRESULT)0x80070057L)
#define E_OUTOFMEMORY   ((HRESULT)0x8007000EL)
#define E_NOTIMPL       ((HRESULT)0x80004001L)
#define E_NOINTERFACE   ((HRESULT)0x80004002L)

#define FAILED(hr)   (((HRESULT)(hr)) < 0)
#define SUCCEEDED(hr)(((HRESULT)(hr)) >= 0)

// ---------------------------------------------------------------------------
// Boolean constants
// ---------------------------------------------------------------------------

#ifndef FALSE
#   define FALSE 0
#endif
#ifndef TRUE
#   define TRUE  1
#endif

// ---------------------------------------------------------------------------
// Instance / handle stubs
// The core Orbiter code passes HINSTANCE around to loading/module APIs that
// are Windows-specific.  On non-Windows these are replaced with void* and
// the relevant code paths are conditionally compiled out.
// ---------------------------------------------------------------------------

struct HINSTANCE__ { int unused; };
typedef HINSTANCE__* HINSTANCE;

struct HWND__ { int unused; };
typedef HWND__* HWND;

struct HDC__ { int unused; };
typedef HDC__* HDC;

struct HBITMAP__ { int unused; };
typedef HBITMAP__* HBITMAP;

struct HMENU__ { int unused; };
typedef HMENU__* HMENU;

// ---------------------------------------------------------------------------
// Utility macros / functions
// ---------------------------------------------------------------------------

inline void ZeroMemory(void* dest, std::size_t count) { std::memset(dest, 0, count); }
inline void CopyMemory(void* dest, const void* src, std::size_t count) { std::memcpy(dest, src, count); }

#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
     ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))

// POINT — used in ScreenToClient and similar
struct POINT { LONG x, y; };

// RECT — used in various UI code
struct RECT { LONG left, top, right, bottom; };

// SIZE
struct SIZE { LONG cx, cy; };

// ---------------------------------------------------------------------------
// String / character helpers used in the codebase
// ---------------------------------------------------------------------------

inline int stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline int strnicmp(const char* a, const char* b, std::size_t n) { return strncasecmp(a, b, n); }

// _MAX_PATH equivalent
#ifndef MAX_PATH
#   define MAX_PATH 4096
#endif

#endif // ORBITER_WINTYPES_H
