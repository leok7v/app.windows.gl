#pragma once

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

typedef struct application_s {
    void (*start)(int argc, const char** argv);
    void (*window_opened)(); // called on window close
    void (*window_closed)(); // called on window close
    void (*window_destroyed)(); // called on window close
    int  (*stop)();  // returns exit code
    void (*keyboard_input)(int character);
    void (*paint)();
    const char* (*id)();
    const char* (*title)();
    void (*min_size)(int* w, int* h);
    void (*max_size)(int* w, int* h);
} application_t;

extern application_t application;

#define STRINGIFY1(x) #x
#define STRINGIFY2(x) STRINGIFY1(x)

#ifdef _MSC_VER // "extern" because of: https://connect.microsoft.com/VisualStudio/Feedback/Details/1587892
#define static_code void _init_##n(void); extern void (*_init_ ## __FILE__ ## __LINE__ ## _)(void); __pragma(section(".CRT$XCU", read)) __declspec(allocate(".CRT$XCU")) void (*_init_##n##_)(void) = _init_##n; void _init_##n(void)
#else
#define static_code __attribute__((constructor)) static void _init_ ## __FILE__ ## __LINE__ ## _)(void)
#endif


BEGIN_C
END_C
