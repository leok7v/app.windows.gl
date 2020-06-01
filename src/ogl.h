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

#define gl_check(call) call; { \
    int _gl_error_ = glGetError(); \
    if (_gl_error_ != 0) { printf("%s(%d): %s %s glError=%d\n", __FILE__, __LINE__, __func__, #call, _gl_error_); } \
}

void glOrtho2D_np(float* mat, float left, float right, float bottom, float top); // GL ES does not have it

END_C

#ifdef IMPLEMENT_OGL

BEGIN_C

void glOrtho2D_np(float* mx, float left, float right, float bottom, float top) {
    // this is straight from http://en.wikipedia.org/wiki/Orthographic_projection_(geometry)
    const float znear = -1;
    const float zfar  =  1;
    const float inv_z = 1 / (zfar - znear);
    const float inv_y = 1 / (top - bottom);
    const float inv_x = 1 / (right - left);
    mx[0] = 2 * inv_x; mx[1] = 0; mx[2] = 0; mx[3] = 0;
    mx[4] = 0; mx[5] = 2 * inv_y; mx[6] = 0; mx[7] = 0;
    mx[8] = 0; mx[9] = 0; mx[10] = -2 * inv_z; mx[11] = 0;
    mx[12] = -(right + left) * inv_x; mx[13] = -(top + bottom) * inv_y;
    mx[14] = -(zfar + znear) * inv_z; mx[15] = 1;
}

END_C

#endif // IMPLEMENT_OGL
