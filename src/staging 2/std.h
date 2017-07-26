#pragma once

#if defined(WIN32) || defined(_WINDOWS)
#define WINDOWS
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifndef WINDOWS
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#endif

#ifdef __cplusplus
#define BEGIN_C extern "C" {
#define END_C } // extern "C"
#define null    nullptr
#else
#define BEGIN_C
#define END_C  
#define null    ((void*)0)
#ifndef false
#define false               0
#endif
#ifndef true
#define true                1
#endif
#endif

#if defined(byte) &&  defined(WINDOWS)
#define byte BYTE
#else
#define byte uint8_t
#endif

#ifdef _MSC_VER // "extern" because of: https://connect.microsoft.com/VisualStudio/Feedback/Details/1587892
#define static_init__(n, line) void _init_ ## n ## _ ## line(void); extern void (*_init_ ## n ## _ ## line ## _)(void); __pragma(section(".CRT$XCU", read)) __declspec(allocate(".CRT$XCU")) void (*_init_ ## n ## _ ## line ## _)(void) = _init_ ## n ## _ ## line; void _init_##n ## _ ## line(void)
#else
#define static_init__(n, line) __attribute__((constructor)) static void _init_ ## n ## _ ## line(void)
#endif
#define static_init_(n, line) static_init__(n, line)
#define static_init(n) static_init_(n, __LINE__)

#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif
#if defined(DEBUG) && !defined(_DEBUG)
#define _DEBUG
#endif

#ifdef IMPLEMENT_STD

BEGIN_C


END_C

#endif // IMPLEMENT_STD
