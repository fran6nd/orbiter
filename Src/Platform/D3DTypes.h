// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ============================================================================
// D3DTypes.h
// Cross-platform definitions of the Direct3D 7 / DDraw types that are used
// in Orbiter's core math and mesh code (D3dmath.h, D3d7util.h, Baseobj.h,
// Camera.cpp, Mesh.cpp, Pane.cpp, Shadow.h …).
//
// These types are layout-compatible with the real DirectX structures so that
// serialised data and binary assets remain interchangeable.  No DirectX SDK
// is required.
//
// Only included on non-Windows platforms via Platform.h / D3DTypes.h guard.
// On Windows the real DirectX headers supply these definitions.
// ============================================================================

#ifndef ORBITER_D3DTYPES_H
#define ORBITER_D3DTYPES_H

#if defined(_WIN32) || defined(_WIN64)
#   error "D3DTypes.h must not be included on Windows — include <d3d.h> instead."
#endif

#include "WinTypes.h"

// ---------------------------------------------------------------------------
// Scalar / colour types
// ---------------------------------------------------------------------------

typedef float  D3DVALUE;
typedef DWORD  D3DCOLOR;

// ---------------------------------------------------------------------------
// Vector
// ---------------------------------------------------------------------------

struct D3DVECTOR {
    D3DVALUE x, y, z;
};

// ---------------------------------------------------------------------------
// 4×4 row-major matrix — members named to match D3DMATRIX exactly.
// The union gives both named (_11 … _44) and flat-array access.
// ---------------------------------------------------------------------------

struct D3DMATRIX {
    union {
        struct {
            D3DVALUE _11, _12, _13, _14;
            D3DVALUE _21, _22, _23, _24;
            D3DVALUE _31, _32, _33, _34;
            D3DVALUE _41, _42, _43, _44;
        };
        D3DVALUE m[4][4];
    };
};

// ---------------------------------------------------------------------------
// Flexible vertex format flags (FVF) — bit constants only, no D3D API needed.
// ---------------------------------------------------------------------------

#define D3DFVF_XYZ        0x0002
#define D3DFVF_XYZRHW     0x0004
#define D3DFVF_NORMAL     0x0010
#define D3DFVF_DIFFUSE    0x0040
#define D3DFVF_SPECULAR   0x0080
#define D3DFVF_TEX1       0x0100
#define D3DFVF_TEX2       0x0200
#define D3DFVF_TEXCOORDSIZE2(n) 0  // no-op on non-Windows; value unused by math code

// ---------------------------------------------------------------------------
// Vertex structures from D3d7util.h — reproduced here so that D3d7util.h can
// guard its <d3d.h> include and still be used on non-Windows.
// ---------------------------------------------------------------------------

struct VECTOR2D     { D3DVALUE x, y; };

struct VERTEX_XYZ   { D3DVALUE x, y, z; };
struct VERTEX_XYZH  { D3DVALUE x, y, z, h; };
struct VERTEX_XYZC  { D3DVALUE x, y, z; D3DCOLOR col; };
struct VERTEX_XYZHC { D3DVALUE x, y, z, h; D3DCOLOR col; };

struct VERTEX_2TEX {
    D3DVALUE x, y, z, nx, ny, nz;
    D3DVALUE tu0, tv0, tu1, tv1;
    inline VERTEX_2TEX() {}
    inline VERTEX_2TEX(D3DVECTOR p, D3DVECTOR n,
                       D3DVALUE u0, D3DVALUE v0,
                       D3DVALUE u1, D3DVALUE v1)
    { x=p.x; y=p.y; z=p.z; nx=n.x; ny=n.y; nz=n.z;
      tu0=u0; tv0=v0; tu1=u1; tv1=v1; }
};

struct VERTEX_TL1TEX {
    D3DVALUE x, y, z, rhw;
    D3DCOLOR col;
    D3DVALUE tu, tv;
};

// ---------------------------------------------------------------------------
// POSTEXVERTEX (used in D3dmath.h)
// ---------------------------------------------------------------------------

struct POSTEXVERTEX {
    D3DVALUE x, y, z;
    D3DVALUE tu, tv;
};

// Placeholder FVF constant; only the struct layout matters on non-Windows.
#define POSTEXVERTEXFLAG 0

// ---------------------------------------------------------------------------
// epsilon used in D3dmath.cpp
// ---------------------------------------------------------------------------

#ifndef g_EPSILON
constexpr float g_EPSILON = 1.0e-5f;
#endif

#endif // ORBITER_D3DTYPES_H
