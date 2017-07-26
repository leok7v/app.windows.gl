#pragma once

/* REQUIRED:
#include "std.h"
#include "ogl.h"
#include "mem.h"
*/

typedef struct app_size_s {
    int w;     int h; 
    int min_w; int min_h; 
    int max_w; int max_h;
} app_size_t;

#ifdef IMPLEMENT_APP

BEGIN_C

int  app_main(void* context, int argc, const char** argv);
void app_size(app_size_t* sizes);
void app_max_size(int* w, int* h);
const char* app_title();
void app_paint();
void app_keyboard(int state, int key, int character);
void app_pointer(int state, int index, int x, int y, float pressure, float proximity); // index of multitouch 'finger'

void app_quit(int result); // PostQuitMessage()
void app_exit(int result); // ExitProcess()

END_C

#ifdef WINDOWS 

#if !defined(STRICT)
#define STRICT
#endif
#define WIN32_LEAN_AND_MEAN

#pragma warning(disable: 4820) // '...' bytes padding added after data member
#pragma warning(disable: 4668) // '...' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning(disable: 4917) // '...' : a GUID can only be associated with a class, interface or namespace
#pragma warning(disable: 4987) // nonstandard extension used: 'throw (...)'
#pragma warning(disable: 4365) // argument : conversion from 'int' to 'size_t', signed/unsigned mismatch
#pragma warning(error:   4706) // assignment in conditional expression (this is the only way I found to turn it on)
#include <Windows.h>
//#pragma comment(lib, "shell32")

BEGIN_C

static HWND window;
static HGLRC glrc;
static HCURSOR CURSOR_ARROW;
static HCURSOR CURSOR_WAIT;

static int set_pixel_format(HWND win, int color_bits, int alpha_bits, int depth_bits, int stencil_bits, int accum_bits) {
   HDC dc = GetDC(win);
   PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
   pfd.nVersion = 1;
   pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
   pfd.dwLayerMask = PFD_MAIN_PLANE;
   pfd.iPixelType = PFD_TYPE_RGBA;
   pfd.cColorBits = (byte)color_bits;
   pfd.cAlphaBits = (byte)alpha_bits;
   pfd.cDepthBits = (byte)depth_bits;
   pfd.cStencilBits = (byte)stencil_bits;
   pfd.cAccumBits = (byte)accum_bits;
   int pixel_format = ChoosePixelFormat(dc, &pfd);
   if (pixel_format == 0) {
       return false;
   }
   if (!DescribePixelFormat(dc, pixel_format, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
      return false;
   }
   SetPixelFormat(dc, pixel_format, &pfd);
   return true;
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCDESTROY) { return DefWindowProcA(window, msg, wp, lp); } // because GetWindowLongPtr won't work
    switch (msg) {
        case WM_GETMINMAXINFO: {
            MINMAXINFO* minmaxinfo = (MINMAXINFO*)lp;
            app_size_t sizes = {0};
            app_size(&sizes);
            minmaxinfo->ptMaxTrackSize.x = minmaxinfo->ptMaxSize.x = sizes.max_w;
            minmaxinfo->ptMaxTrackSize.y = minmaxinfo->ptMaxSize.y = sizes.max_h;
            minmaxinfo->ptMinTrackSize.x = sizes.min_w;
            minmaxinfo->ptMinTrackSize.y = sizes.min_h;
            break;
        }
        case WM_CLOSE        : PostMessage(window, 0xC003, 0, 0);  return 0; // w/o calling DefWindowProc not to DestroyWindow() prematurely
        case 0xC003          : /* now all the messages has been processed in the queue */ DestroyWindow(window); return 0; 
        case WM_DESTROY      : PostQuitMessage(0); break;
        case WM_CHAR         : app_keyboard(0, 0, (int)wp); break;
        case WM_PAINT        : {
            PAINTSTRUCT ps = {null};
            BeginPaint(window, &ps);
//          SelectObject(ps.hdc, GetStockObject(WHITE_BRUSH));
//          Rectangle(ps.hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
            SelectObject(ps.hdc, GetStockObject(NULL_BRUSH));
            app_paint(); 
            SwapBuffers(ps.hdc);
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
        case WM_CREATE: {
            static const int color_bits = 24;
            static const int alpha_bits = 0;
            static const int depth_bits = 24;
            static const int stencil_bits = 0;
            static const int accum_bits = 0;
            if (set_pixel_format(window, color_bits, alpha_bits, depth_bits, stencil_bits, accum_bits)) {
                glrc = wglCreateContext(GetDC(window));
                if (glrc != null && wglMakeCurrent(GetDC(window), glrc)) {
                    PostThreadMessage(GetCurrentThreadId(), 0xC002, 0, 0);
                    return 0;
                }
            }
            assert(false);
            ExitProcess(0x1BADF00D);
        }
        case WM_NCHITTEST    : {
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

static void create_window() {
    CURSOR_ARROW = LoadCursor(null, IDC_ARROW);
    CURSOR_WAIT  = LoadCursor(null, IDC_WAIT);
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW|CS_VREDRAW|CS_DROPSHADOW|CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 256*1024;
    wc.hInstance = GetModuleHandle(null); 
#ifndef IDI_ICON
    wc.hIcon = LoadIcon(GetModuleHandle(null), MAKEINTRESOURCE(101)); // MS Dev Studio usualy generates IDI_ICON as 101
#else
    wc.hIcon = LoadIcon(GetModuleHandle(null), MAKEINTRESOURCE(IDI_ICON));
#endif
    wc.hCursor = CURSOR_ARROW;
    wc.hbrBackground = null; // CreateSolidBrush(RGB(0, 0, 0)); 
    wc.lpszMenuName = null;
    wc.lpszClassName = "app";
    ATOM atom = RegisterClassA(&wc);
    assert(atom != 0);
    app_size_t sizes = {0};
    app_size(&sizes);
    window = atom == 0 ? null :
               CreateWindowExA(WS_EX_COMPOSITED|WS_EX_LAYERED|WS_EX_APPWINDOW, 
                               wc.lpszClassName, "", /*WS_BORDER*/0, 
                               -1, -1, sizes.w, sizes.h, null, null, GetModuleHandle(null), null);
    assert(atom == 0 || window != null);
    if (window != null) {
        SetWindowLong(window, GWL_STYLE, 0);
        ShowWindow(window, SW_HIDE);
    } else {
        ExitProcess(ERROR_FATAL_APP_EXIT);
    }
}

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

int main(int argc, const char** argv) {
    return app_main(null, argc, argv);
}

static HANDLE pipe_stdout_write;
static HANDLE pipe_stdout_read;

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ char*, _In_ int show_command) {
    const char* cl = GetCommandLineA();
    const int len = (int)strlen(cl);
    const int k = ((len + 2) / 2) * sizeof(void*) + sizeof(void*);
    const int n  = k + (len + 2) * sizeof(char);
    const char** argv = (const char**)alloca(n);
    memset(argv, 0, n);
    char* buff = (char*)(((char*)argv) + k);
    int argc = parse_argv(cl, argv, buff);
    MSG msg = {0};
    CreatePipe(&pipe_stdout_read, &pipe_stdout_write, null, 0);
    SetStdHandle(STD_OUTPUT_HANDLE, pipe_stdout_write);
    SetStdHandle(STD_ERROR_HANDLE, pipe_stdout_write);
    int fd = _open_osfhandle((intptr_t)pipe_stdout_write, _O_TEXT);
    if (fd==-1) {
        assert(false);
    };
    if (_dup2(fd, 1) != 0) { assert(false); };
    if (_dup2(fd, 2) != 0) { assert(false); };
    char buffer[16 * 1024 + 1];
    PostThreadMessage(GetCurrentThreadId(), 0xC001, 0, 0);
    while (GetMessage(&msg, null, 0, 0)) {
        DWORD available = 0;
        if (PeekNamedPipe(pipe_stdout_read, buffer, sizeof(buffer) - 1, null, &available, null) && available > 0) {
//          sprintf(buffer, "available=%d\n", available);
            DWORD read = 0;
            if (ReadFile(pipe_stdout_read, buffer, sizeof(buffer) - 1, &read, null) && read > 0) {
                buffer[read] = 0;
                OutputDebugStringA(buffer);
            }
        }
        if (msg.message == 0xC001) {
            create_window();
        } else if (msg.message == 0xC002) {
            int r = app_main(window, argc, argv);
            if (r != 0) {
                PostQuitMessage(r);                
            } else {
                SetWindowTextA(window, app_title());
                ShowWindow(window, show_command);
            }
        } else {
            TranslateMessage(&msg); // WM_KEYDOWN/UP -> WM_CHAR, WM_CLICK, WM_CLICK -> WM_DBLCLICK ...
            DispatchMessage(&msg);
        }
    }
    CloseHandle(pipe_stdout_read);
    CloseHandle(pipe_stdout_write);
    assert(msg.message == WM_QUIT);
    return (int)msg.wParam;
}

void app_quit(int result) {
    PostQuitMessage(result);
}

void app_exit(int result) {
    ExitProcess(result);
}

#ifdef DEBUG

#include "crtdbg.h"

static _CrtMemState s1;

static void app_heap_leaks_detect() {
    _CrtMemState s2, s3;
    _CrtMemCheckpoint(&s2);
    if (_CrtMemDifference(&s3, &s1, &s2)) {
        _CrtMemDumpStatistics(&s3);
        // we want to dump memory leaks earlier to enable memory
        // examination when we heat breakpoint below
        _CrtDumpMemoryLeaks(); 
        _CrtSetDbgFlag(0); // not to dump leaks twice     
//      __debugbreak();    // this is your chance to examine content of leaked memory in Alt+6 Debugger Memory View pane
    }
}

static_init(app_heap_leaks_detection) {
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
    _CrtMemCheckpoint(&s1);
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

#endif // WINDOWS


#endif // IMPLEMENT_APP
