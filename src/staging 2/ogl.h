#pragma once

#include "std.h"
// Open GL

#ifdef _MSC_VER

#ifndef WINGDIAPI
#define CALLBACK    __stdcall
#define WINGDIAPI   __declspec(dllimport)
#define APIENTRY    __stdcall
#define CLEANUP_WINGDIAPI_DEFINES
#endif
#include <gl/gl.h>
#include <gl/glu.h>
#ifdef CLEANUP_WINGDIAPI_DEFINES
#undef CALLBACK    
#undef WINGDIAPI   
#undef APIENTRY    
#endif
#else
#include <gl/gl.h>
#include <gl/glu.h>
#endif

BEGIN_C

void glOrtho2D_np(float* mat, float left, float right, float bottom, float top); // GL ES does not have it

END_C

#ifdef IMPLEMENT_OGL

BEGIN_C
void glOrtho2D_np(float* mat, float left, float right, float bottom, float top) {
    // this is basically from
    // http://en.wikipedia.org/wiki/Orthographic_projection_(geometry)
    const float znear = -1.0f;
    const float zfar  =  1.0f;
    const float inv_z = 1.0f / (zfar - znear);
    const float inv_y = 1.0f / (top - bottom);
    const float inv_x = 1.0f / (right - left);
    // first column
    *mat++ = 2.0f * inv_x;
    *mat++ = 0.0f;
    *mat++ = 0.0f;
    *mat++ = 0.0f;
    // second
    *mat++ = 0.0f;
    *mat++ = 2.0f * inv_y;
    *mat++ = 0.0f;
    *mat++ = 0.0f;
    // third
    *mat++ = 0.0f;
    *mat++ = 0.0f;
    *mat++ = -2.0f * inv_z;
    *mat++ = 0.0f;
    // fourth
    *mat++ = -(right + left) * inv_x;
    *mat++ = -(top + bottom) * inv_y;
    *mat++ = -(zfar + znear) * inv_z;
    *mat   = 1.0f;
}

END_C

#endif // IMPLEMENT_OGL
