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
#ifndef _MSC_VER
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

BEGIN_C

#if defined(byte) &&  defined(_MSC_VER)
#define byte BYTE
#else
#define byte uint8_t
#endif

#ifdef _MSC_VER
#define thread_local_storage __declspec(thread)
#endif
#define countof(a) (sizeof((a)) / sizeof((a)[0]))

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

extern FILE* stdbug; // implemented by platform can be opened to /dev/null

const char* strerr(int r); // extended platform specific strerror()

#define traceln(format, ...) fprintf(stdbug, "%s(%d): %s [%04d] " format "\n", __FILE__, __LINE__, __func__, gettid(), __VA_ARGS__)

#ifdef DEBUG
#undef assert
#define assertion(b, format, ...) (void)( (!!(b)) || (traceln("%s(%d): %s assertion " ## #b ## "failed. " format "\n", __FILE__, __LINE__, __func__, __VA_ARGS__), 0))
#define assert(b) assertion(b, "")
#else
#undef assert
#define assertion(b, format, ...) (void)(0)
#define assert(b) (void)(0)
#endif

#ifdef WINDOWS // even if compile with gcc or clang, also define _CRT_SECURE_NO_WARNINGS=1
int gettid();
int getppid();
#define open(...)       _open(__VA_ARGS__)
#define read(...)       _read(__VA_ARGS__)
#define write(...)      _write(__VA_ARGS__)
#define close(...)      _close(__VA_ARGS__)
#define stat(...)       _stat(__VA_ARGS__)
#define fstat(...)      _fstat(__VA_ARGS__)
#define mkdir(...)      _mkdir(__VA_ARGS__)
#define snprintf(...)   _snprintf(__VA_ARGS__)
#define vsnprintf(...)  _vsnprintf(__VA_ARGS__)
#define O_RDONLY        _O_RDONLY
#define O_BINARY        _O_BINARY
#define O_CREAT         _O_CREAT
#define O_WRONLY        _O_WRONLY
#define O_TRUNC         _O_TRUNC
#define S_IREAD         _S_IREAD
#define S_IWRITE        _S_IWRITE
#define S_IFDIR         _S_IFDIR#endif
#endif

END_C
