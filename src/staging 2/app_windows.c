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
#include <assert.h>
#include <stdint.h>
#include "manifest.h"
#include <malloc.h>
#include <ctype.h>
#include "resource.h"

#pragma comment(lib, "shell32")

BEGIN_C

static HCURSOR CURSOR_ARROW;
static HCURSOR CURSOR_WAIT;

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCDESTROY) { return DefWindowProcA(window, msg, wp, lp); } // because GetWindowLongPtr won't work
    switch (msg) {
        case WM_GETMINMAXINFO: {
            MINMAXINFO* minmaxinfo = (MINMAXINFO*)lp;
            int max_width = 0;
            int max_height = 0;
            application.max_size(&max_width, &max_height);
            minmaxinfo->ptMaxTrackSize.x = minmaxinfo->ptMaxSize.x = max_width;
            minmaxinfo->ptMaxTrackSize.y = minmaxinfo->ptMaxSize.y = max_width;
            int min_width = 0;
            int min_height = 0;
            application.min_size(&min_width, &min_height);
            minmaxinfo->ptMinTrackSize.x = max(min_width, 640);
            minmaxinfo->ptMinTrackSize.y = max(min_height, 480);
            break;
        }
        case WM_CLOSE        : application.window_closed(); 
                               PostMessage(window, 0xC003, 0, 0); 
                               return 0; // w/o calling DefWindowProc not to DestroyWindow() prematurely
        case 0xC003          : /* now all the messages has been processed in the queue */ DestroyWindow(window); return 0; 
        case WM_DESTROY      : application.window_destroyed(); 
                               PostQuitMessage(0); break;
        case WM_CHAR         : application.keyboard_input((int)wp); break;
        case WM_PAINT        : {
            PAINTSTRUCT ps = {0};
            BeginPaint(window, &ps);
            application.paint(); 
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
        default: break;
    }
    return DefWindowProcA(window, msg, wp, lp);
}

static void create_window(int show_command) {
    CURSOR_ARROW = LoadCursor(null, IDC_ARROW);
    CURSOR_WAIT  = LoadCursor(null, IDC_WAIT);
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW|CS_VREDRAW|CS_DROPSHADOW|CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 256*1024;
    wc.hInstance = GetModuleHandle(null); 
    wc.hIcon = LoadIcon(GetModuleHandle(null), MAKEINTRESOURCE(IDI_ICON));
    wc.hCursor = CURSOR_ARROW;
    wc.hbrBackground = null; // CreateSolidBrush(RGB(0, 0, 0)); 
    wc.lpszMenuName = null;
    wc.lpszClassName = application.id();
    ATOM atom = RegisterClassA(&wc);
    assert(atom != 0);
    int min_width = 0;
    int min_height = 0;
    application.min_size(&min_width, &min_height);
    HWND window = atom == 0 ? null :
               CreateWindowExA(WS_EX_COMPOSITED|WS_EX_LAYERED, 
                               wc.lpszClassName, application.title(), 
                               WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN, 0, 0, 
                               max(min_width, 640) + GetSystemMetrics(SM_CXSIZEFRAME) * 2, 
                               max(min_height, 480) + GetSystemMetrics(SM_CXSIZEFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION), 
                               null, null, GetModuleHandle(null), &application);
    assert(atom == 0 || window != null);
    if (window != null) {
        ShowWindow(window, show_command);
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

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ char*, _In_ int show_command) {
#ifdef DEBUG
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
    _CrtMemState s1;
    _CrtMemCheckpoint(&s1);
    _CrtSetDbgFlag((_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF) & ~_CRTDBG_CHECK_CRT_DF);
//  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_CHECK_ALWAYS_DF);
/*
    _crtBreakAlloc = 241; // {241} crt block at 0x012B13E8, subtype 0, 24 bytes long
    _CrtSetBreakAlloc(241);
*/
#endif
    const char* cl = GetCommandLineA();
    const int len = strlen(cl);
    const int k = ((len + 2) / 2) * sizeof(void*) + sizeof(void*);
    const int n  = k + (len + 2) * sizeof(char);
    const char** argv = (const char**)alloca(n);
    memset(argv, 0, n);
    char* buff = (char*)(((char*)argv) + k);
    int argc = parse_argv(cl, argv, buff);
    application.start(argc, argv);
    MSG msg = {0};
    PostThreadMessage(GetCurrentThreadId(), 0xC001, 0, (LPARAM)(uintptr_t)&application);
    while (GetMessage(&msg, null, 0, 0)) {
        if (msg.message == 0xC001) {
            create_window(show_command);
            application.window_opened();
        } else {
            TranslateMessage(&msg); // WM_KEYDOWN/UP -> WM_CHAR, WM_CLICK, WM_CLICK -> WM_DBLCLICK ...
            DispatchMessage(&msg);
        }
    }
    assert(msg.message == WM_QUIT);
    int r = application.stop();
#ifdef DEBUG
//  void* intentinal_leak_for_testing = malloc(153); (void)intentinal_leak_for_testing;
    _CrtMemState s2, s3;
    _CrtMemCheckpoint(&s2);
    if (_CrtMemDifference(&s3, &s1, &s2)) {
        _CrtMemDumpStatistics(&s3);
        // we want to dump memory leaks earlier to enable memory
        // examination when we heat breakpoint below
        _CrtDumpMemoryLeaks(); 
        _CrtSetDbgFlag(0); // not to dump leaks twice     
//      __debugbreak();    // this is your chance to examine leaked memory in Alt+6 Debugger Memory View pane
    }
#endif
    return r == 0 ? (int)msg.wParam : r;
}


END_C
