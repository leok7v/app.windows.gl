#pragma once

#include "app.h"

#if !defined(STRICT)
#define STRICT
#endif

#define WIN32_LEAN_AND_MEAN 
#define VC_EXTRALEAN
#define NOMINMAX 
#pragma warning(disable: 4820) // '...' bytes padding added after data member
#pragma warning(disable: 4668) // '...' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning(disable: 4917) // '...' : a GUID can only be associated with a class, interface or namespace
#pragma warning(disable: 4987) // nonstandard extension used: 'throw (...)'
#pragma warning(disable: 4365) // argument : conversion from 'int' to 'size_t', signed/unsigned mismatch
#pragma warning(error:   4706) // assignment in conditional expression (this is the only way I found to turn it on)
#include <Windows.h>

BEGIN_C

FILE* stdtrace;

typedef struct context_s {
    app_t app;
    HWND window;
    HGLRC glrc;
    HCURSOR CURSOR_ARROW;
    HCURSOR CURSOR_WAIT;
    HANDLE pipe_stdout_read;
    HANDLE pipe_stdout_write;
    HANDLE logger_thread;
    volatile bool quiting;
} context_t;

const char* strerr(int r) {
    thread_local_storage static char text[4 * 1024];
    if (r == 0) { return ""; }
    if (0 <= r && r <= 42) {
        _snprintf(text, countof(text), "%s", strerror(r));
    } else {
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK, null, r, 0, text, _countof(text), null);
    }
    return text;
}

static int set_pixel_format(HWND win, int color_bits, int alpha_bits, int depth_bits, int stencil_bits, int accum_bits) {
    HDC dc = GetWindowDC(win);
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
    pfd.nVersion = 1;
    pfd.dwFlags  = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pfd.dwLayerMask  = PFD_MAIN_PLANE;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = (byte)color_bits;
    pfd.cAlphaBits   = (byte)alpha_bits;
    pfd.cDepthBits   = (byte)depth_bits;
    pfd.cStencilBits = (byte)stencil_bits;
    pfd.cAccumBits   = (byte)accum_bits;
    int pixel_format = ChoosePixelFormat(dc, &pfd);
    if (pixel_format == 0) {
        return false;
    }
//  if (!DescribePixelFormat(dc, pixel_format, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) { return false; }
    bool b = SetPixelFormat(dc, pixel_format, &pfd);
    ReleaseDC(win, dc);
    return b;
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    context_t* context = (context_t*)GetPropA(window, "app.context");
    app_t* app = context != null ? &context->app : null;
    if (msg == WM_NCDESTROY) { return DefWindowProcA(window, msg, wp, lp); } // because GetWindowLongPtr won't work
    switch (msg) {
        case WM_GETMINMAXINFO: {
            MINMAXINFO* minmaxinfo = (MINMAXINFO*)lp;
            minmaxinfo->ptMaxTrackSize.x = minmaxinfo->ptMaxSize.x = app->max_w;
            minmaxinfo->ptMaxTrackSize.y = minmaxinfo->ptMaxSize.y = app->max_h;
            minmaxinfo->ptMinTrackSize.x = app->min_w;
            minmaxinfo->ptMinTrackSize.y = app->min_h;
            break;
        }
        case WM_CLOSE        : PostMessage(window, 0xC003, 0, 0);  return 0; // w/o calling DefWindowProc not to DestroyWindow() prematurely
        case 0xC003          : /* now all the messages has been processed in the queue */ DestroyWindow(window); return 0; 
        case WM_DESTROY      : PostQuitMessage(0); break;
        case WM_CHAR         : app->keyboard(app, 0, 0, (int)wp); break;
        case WM_PAINT        : {
            PAINTSTRUCT ps = {};
            RECT rc;
            BeginPaint(window, &ps);
            SelectObject(ps.hdc, GetStockObject(NULL_BRUSH));
            GetUpdateRect(window, &rc, false);
            HDC wdc = GetWindowDC(window);
            wglMakeCurrent(wdc, context->glrc);
            app->paint(app, rc.top, rc.left, rc.right - rc.left, rc.bottom - rc.top);
            if (!SwapBuffers(wdc)) {
                printf("%d %s", GetLastError(), strerr(GetLastError()));
            }
            printf("wdc=%p pdc=%p\n", wdc, ps.hdc);
            ReleaseDC(window, wdc);
            EndPaint(window, &ps);
        }
        case WM_ERASEBKGND   : return true;
//      case WM_SETCURSOR    : SetCursor(window->cursor); break;
/*
        case WM_MOUSEMOVE    :
        case WM_LBUTTONDOWN  :
        case WM_LBUTTONUP    :
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN  :
        case WM_RBUTTONUP    :
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN  :
        case WM_MBUTTONUP    :
        case WM_MBUTTONDBLCLK: application.on_mouse(app, window, (int)msg, (int)wp, (int)GET_X_LPARAM(lp), (int)GET_Y_LPARAM(lp)); break;
        case WM_KEYDOWN      : application.on_key_down(app, window, (int)wp); break;
        case WM_KEYUP        : application.on_key_up(app, window, (int)wp); break;
        case WM_TIMER        : application.on_timer(app, window, (int)wp); break;
*/
        case WM_CREATE: 
            PostThreadMessage(GetCurrentThreadId(), 0xC002, 0, 0);
            return 0;
        case WM_NCHITTEST: {
            POINT pt = {LOWORD(lp), HIWORD(lp)};
            RECT rc = {0};
            GetClientRect(window, &rc);
            const int w = rc.right - rc.left;
            const int h = rc.bottom - rc.top;
            ScreenToClient(window, &pt);
            if (pt.x < 8 && pt.y < 8) { return HTTOPLEFT; }
            if (pt.x > w - 8 && pt.y < 8) { return HTTOPRIGHT; }
            if (pt.x < 8 && pt.y > h - 8) { return HTBOTTOMLEFT; }
            if (pt.x > w - 8 && pt.y > h - 8) { return HTBOTTOMRIGHT; }
            if (pt.x < 8) { return HTLEFT; }
            if (pt.x > w - 8) { return HTRIGHT; }
            if (pt.y < 8) { return HTTOP; }
            if (pt.y > h - 8) { return HTBOTTOM; }
            return pt.y < 40 ? HTCAPTION : 0;
        }
        
        default: break;
    }
    return DefWindowProcA(window, msg, wp, lp);
}

static void create_window(context_t* context) {
    context->CURSOR_ARROW = LoadCursor(null, IDC_ARROW);
    context->CURSOR_WAIT  = LoadCursor(null, IDC_WAIT);
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; //  | CS_DROPSHADOW
    wc.lpfnWndProc = window_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 256*1024;
    wc.hInstance = GetModuleHandle(null); 
#ifndef IDI_ICON
    wc.hIcon = LoadIcon(GetModuleHandle(null), MAKEINTRESOURCE(101)); // MS Dev Studio usualy generates IDI_ICON as 101
#else
    wc.hIcon = LoadIcon(GetModuleHandle(null), MAKEINTRESOURCE(IDI_ICON));
#endif
    wc.hCursor = context->CURSOR_ARROW;
    wc.hbrBackground = null; // CreateSolidBrush(RGB(0, 0, 0)); 
    wc.lpszMenuName = null;
    wc.lpszClassName = "app";
    ATOM atom = RegisterClassA(&wc);
    assert(atom != 0);
    context->window = atom == 0 ? null :
               CreateWindowExA(0 | WS_EX_APPWINDOW, // WS_EX_COMPOSITED|WS_EX_LAYERED|WS_EX_APPWINDOW,
                               wc.lpszClassName, "", /*WS_BORDER|WS_VISIBLE*/ WS_POPUP,
                               context->app.x, context->app.y, context->app.w, context->app.h, 
                               null, null, GetModuleHandle(null), null);
    assert(atom == 0 || context->window != null);
    if (context->window == null) {
        ExitProcess(ERROR_FATAL_APP_EXIT);
    } else {
        SetPropA(context->window, "app.context", context);
        ShowWindow(context->window, SW_HIDE);
    }
}

static DWORD WINAPI logger(void* param) {
    SetThreadDescription(GetCurrentThread(), L"logger");
    context_t* context = (context_t*)param;
    char text[2 * 1024 + 1]; // OutputDebugStringA used to have 2KB limitation
    while (!context->quiting) {
        DWORD available = 0;
        if (PeekNamedPipe(context->pipe_stdout_read, text, sizeof(text) - 1, null, &available, null) && available > 0) {
//          sprintf(buffer, "available=%d\n", available);
            DWORD read = 0;
            if (ReadFile(context->pipe_stdout_read, text, sizeof(text) - 1, &read, null) && read > 0) {
                text[read] = 0;
                if (text[0] == 0x00 && text[1] == 'Q' && text[2] == 'U' && text[3] == 'I' && text[4] == 'T') { break; }
                OutputDebugStringA(text);
            }
        }
    }
    return 0;
}

// https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/
// https://msdn.microsoft.com/en-us/library/a1y7w461.aspx
// try executing with:  results should be:
// command line 	    argv[1]	    argv[2]	    argv[3]
// "a b c" d e	        a b c	    d	        e
// "ab\"c" "\\" d	    ab"c	    \	        d
// a\\\b d"e f"g h	    a\\\b	    de fg	    h
// a\\\"b c d	        a\"b	    c	        d
// a\\\\"b c" d e	    a\\b c	    d	        e

static const char BACKSLASH = '\\';
static const char QUOTE = '\"';

static char next_char(const char** cl, int* escaped) {
    char ch = **cl; (*cl)++;
    *escaped = false;
    if (ch == BACKSLASH) {
        if (**cl == BACKSLASH) { (*cl)++; *escaped = true; }
        else if (**cl == QUOTE) { ch = QUOTE; (*cl)++; *escaped = true; }
        else { /* keep the backslash and copy it into the resulting argument */ }
    }
    return ch;
}

static int parse_argv(const char* cl, const char** argv, char* buff) {
    int escaped = 0;
    int argc = 0;
    int j = 0;
    char ch = next_char(&cl, &escaped);
    while (ch != 0) {
        while (isspace(ch)) { ch = next_char(&cl, &escaped); }
        if (ch == 0) { break; }
        argv[argc++] = buff + j;
        if (ch == QUOTE) {
            ch = next_char(&cl, &escaped);
            while (ch != 0) { 
                if (ch == QUOTE && !escaped) { break; }
                buff[j++] = ch; ch = next_char(&cl, &escaped); 
            }
            buff[j++] = 0;
            if (ch == 0) { break; }
            ch = next_char(&cl, &escaped); // skip closing quote maerk
        } else {
            while (ch != 0 && !isspace(ch)) { buff[j++] = ch; ch = next_char(&cl, &escaped); }
            buff[j++] = 0;
        }
    }
    return argc;
}

static int message_box(app_t* a, int flags, const char* format, va_list vl) {
    context_t* context = (context_t*)a;
    char text[32 * 1024];
    _vsnprintf(text, countof(text), format, vl);
    // TODO: should use MessageBoxIndirectA() instead and a bit more complex message dispatch
    return MessageBoxA(context->window, text, a->title, flags);
}

static int app_message_box(app_t* a, int flags, const char* format, ...) {
    va_list vl;
    va_start(format, vl);
    int r = message_box(a, flags, format, vl);
    va_end(vl);
    return r;
}

static void app_toast(app_t* a, int seconds, const char* format, ...) { // TODO: this is not really a toast
    (void)seconds; // unused
    va_list vl;
    va_start(format, vl);
    (void)message_box(a, 0, format, vl);
    va_end(vl);
}

static void app_asset(app_t* a, const char* name, void* *data, int *bytes) { (void)a;
    const HMODULE module = GetModuleHandle(null);
    const HRSRC  res = FindResourceA(module, name, RT_RCDATA);
    const HANDLE handle = res != null ? LoadResource(module, res) : null;
    *data  = handle != null ? LockResource(handle) : null;
    *bytes = handle != null ? (int)SizeofResource(module, res) : 0;
}

static void app_quit(app_t* a, int exit_code)  { (void)a; PostQuitMessage(exit_code); }
static void app_exit(app_t* a, int exit_code)  { (void)a; exit(exit_code); }
static void app_abort(app_t* a, int exit_code) { (void)a; ExitProcess(exit_code); }

static void app_invalidate(app_t* a) {
    context_t* context = (context_t*)a;
    InvalidateRect(context->window, null, false);
}

static void app_invalidate_rectangle(app_t* a, int x, int y, int w, int h) {
    context_t* context = (context_t*)a;
    RECT rc = { x, y, x + w, y + h };
    InvalidateRect(context->window, &rc, false);
}

int app_run(void (*init)(app_t* app)) {
    context_t context = {};
    app_t* app = &context.app;
    app->quit  = app_quit;
    app->exit  = app_exit;
    app->abort = app_abort;
    app->asset = app_asset;
    app->toast = app_toast;
    app->message_box = app_message_box;
    app->invalidate = app_invalidate;
    app->invalidate_rectangle = app_invalidate_rectangle;
    int argc = app->argc;
    const char** argv = app->argv;
    bool console;
    if (argc == 0 && argv == null) {
        console = false;
        const char* cl = GetCommandLineA();
        const int len = (int)strlen(cl);
        const int k = ((len + 2) / 2) * sizeof(void*) + sizeof(void*);
        const int n = k + (len + 2) * sizeof(char);
        argv = (const char**)alloca(n);
        memset(argv, 0, n);
        char* buff = (char*)(((char*)argv) + k);
        argc = parse_argv(cl, argv, buff);
    } else {
        console = true;
        assertion(argc >= 1 && argv != null && argv[0] != null, "argc=%d", argc); // command line invocation
    }
    MSG msg = {};
    CreatePipe(&context.pipe_stdout_read, &context.pipe_stdout_write, null, 0);
    HANDLE std_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE std_err_handle = GetStdHandle(STD_ERROR_HANDLE);
    int fd_trace = _open_osfhandle((intptr_t)context.pipe_stdout_write, _O_WRONLY | _O_TEXT);
    assert(fd_trace > 0);
    stdtrace = _fdopen(fd_trace, "wt");
    setvbuf(stdtrace, null, _IONBF, 0);
    assert(stdtrace != null);
    init(app); // will tell us if it wants stdout, stderr redirected
    if (!console && app->redirect_std) { // do not redirect console handle even if app is begging
        SetStdHandle(STD_OUTPUT_HANDLE, context.pipe_stdout_write);
        SetStdHandle(STD_ERROR_HANDLE, context.pipe_stdout_write);
        if (_fileno(stdout) < 0) { freopen("NUL", "wt", stdout); }
        if (_fileno(stderr) < 0) { freopen("NUL", "wt", stderr); }
        if (_dup2(fd_trace, _fileno(stdout)) != 0) { assert(false); };
        if (_dup2(fd_trace, _fileno(stderr)) != 0) { assert(false); };
        setvbuf(stdout, null, _IONBF, 0);
        setvbuf(stderr, null, _IONBF, 0);
    }
    context.logger_thread = CreateThread(null, 0, logger, &context, 0, null);
    PostThreadMessage(GetCurrentThreadId(), 0xC001, 0, 0);
    while (GetMessage(&msg, null, 0, 0)) {
        if (msg.message == 0xC001) {
            create_window(&context);
        } else if (msg.message == 0xC002) {
            static const int color_bits = 32;
            static const int alpha_bits = 0;
            static const int depth_bits = 24;
            static const int stencil_bits = 8;
            static const int accum_bits = 0;
            HDC dc = GetWindowDC(context.window);
            bool b = set_pixel_format(context.window, color_bits, alpha_bits, depth_bits, stencil_bits, accum_bits);
            context.glrc = b ? wglCreateContext(dc) : null;
            if (context.glrc != null) {
                b = wglMakeCurrent(dc, context.glrc);
            }
            ReleaseDC(context.window, dc);
            assertion(b, "wglCreateContext() failed: %s", strerr(GetLastError()));
            if (!b) { ExitProcess(0x1BADF00D); }
            app->begin(app);
            SetWindowTextA(context.window, app->title != null ? app->title : "");
            ShowWindow(context.window, app->visibility ? SW_SHOW : SW_HIDE);
        } else {
            TranslateMessage(&msg); // WM_KEYDOWN/UP -> WM_CHAR, WM_CLICK, WM_CLICK -> WM_DBLCLICK ...
            DispatchMessage(&msg);
        }
    }
    app->end(app);
    context.quiting = true;
    byte quit = 0;
    WriteFile(context.pipe_stdout_write, &quit, 1, null, null);
    WaitForSingleObject(context.logger_thread, INFINITE); // join thread
    fclose(stdtrace); // will actually close context.pipe_stdout_read
    CloseHandle(context.pipe_stdout_read);
    CloseHandle(context.logger_thread);
    assert(msg.message == WM_QUIT);
    // no need to fclose: stdtrace, stdout and stderr because 
    // they all attached to the same pipe_stdout_write handle which has been closed
    if (app->redirect_std) {
        SetStdHandle(STD_OUTPUT_HANDLE, std_out_handle);
        SetStdHandle(STD_ERROR_HANDLE, std_err_handle);
    }
    return (int)msg.wParam;
}

#ifdef DEBUG

#include "crtdbg.h"

static _CrtMemState memory_state_1;

static void app_heap_leaks_detect() {
    _CrtMemState memory_state_2, memory_state_3;
    _CrtMemCheckpoint(&memory_state_2);
    if (_CrtMemDifference(&memory_state_3, &memory_state_1, &memory_state_2)) {
        _CrtMemDumpStatistics(&memory_state_3);
        // we want to dump memory leaks earlier to enable memory
        // examination when we heat breakpoint below
        _CrtDumpMemoryLeaks(); 
        _CrtSetDbgFlag(0); // not to dump leaks twice     
//      __debugbreak();    // this is your chance to examine content of leaked memory in Alt+6 Debugger Memory View pane
    }
}

static_init(app_heap_leaks_detection) {
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
    _CrtMemCheckpoint(&memory_state_1);
    _CrtSetDbgFlag((_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF) & ~_CRTDBG_CHECK_CRT_DF);
//  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_CHECK_ALWAYS_DF);
//  if we have report like: {241} crt block at 0x012B13E8, subtype 0, 24 bytes long
//  uncommenting next line will set breakpoint on 241th allocation
//  _crtBreakAlloc = 241; _CrtSetBreakAlloc(241);
//  uncomment next line to test that heap leak detection works    
//  void* intentinal_leak_for_testing = mem_alloc(153); (void)intentinal_leak_for_testing;
    atexit(app_heap_leaks_detect);
}

#endif

END_C

