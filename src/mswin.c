#include "app.h"
#include "mswin.h"

BEGIN_C

FILE* stdbug;

typedef struct context_s {
    app_t app;
    HWND window;
    HGLRC glrc;
    HCURSOR CURSOR_ARROW;
    HCURSOR CURSOR_WAIT;
    HANDLE pipe_stdout_read;
    HANDLE pipe_stdout_write;
    HANDLE std_out_handle;
    HANDLE std_err_handle;
    HANDLE logger_thread;
    volatile bool quiting;
    bool subsystem_console; // true for link.exe /SUBSYSTEM:CONSOLE false for /SUBSYSTEM:WINDOWS
    bool parrent_console_attached; // true if app was started from console window
    HWND parent_console_window;
    HANDLE parent_console_input;
    HANDLE parent_console_output;
    HANDLE parent_console_error;
    bool full_screen;
    bool full_screen_saved_maximized;
    int  full_screen_saved_style;
    int  full_screen_saved_ex_style;
    RECT full_screen_saved_rect;
    int  full_screen_saved_presentation;
} context_t;

int gettid() { return GetCurrentThreadId(); }

int getppid() {
    long (WINAPI *NtQueryInformationProcess)(HANDLE processhandle, ULONG processinformationclass,
        void* processinformation, ULONG processinformationlength, ULONG *returnlength);
    *(FARPROC*)&NtQueryInformationProcess = GetProcAddress(LoadLibraryA("NTDLL.DLL"), "NtQueryInformationProcess");
    if (NtQueryInformationProcess != null) {
        uintptr_t pbi[6] = {};
        ULONG size = 0;
        if (NtQueryInformationProcess(GetCurrentProcess(), 0, &pbi, sizeof(pbi), &size) >= 0 && size == sizeof(pbi)) {
            return (int)pbi[5];
        }
    }
    return -1;
}


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

static void* mem_map(const char* filename, int* bytes, bool rw) {
    void* address = null;
    HANDLE file = CreateFileA(filename, rw ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ, 0, null, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, null);
    if (file != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size = { 0 };
        if (GetFileSizeEx(file, &size) && 0 < size.QuadPart && size.QuadPart <= 0x7FFFFFFF) {
            HANDLE map_file = CreateFileMapping(file, NULL, rw ? PAGE_READWRITE : PAGE_READONLY, 0, (DWORD)size.QuadPart, null);
            if (map_file != null) {
                address = MapViewOfFile(map_file, rw ? FILE_MAP_READ | SECTION_MAP_WRITE : FILE_MAP_READ, 0, 0, (int)size.QuadPart);
                if (address != null) {
                    *bytes = (int)size.QuadPart;
                }
                int b = CloseHandle(map_file);
                assert(b); (void)b;
            }
        }
        int b = CloseHandle(file);
        assert(b); (void)b;
    }
    return address;
}

void* mem_map_read(const char* filename, int* bytes) { return mem_map(filename, bytes, false); }

void* mem_map_read_write(const char* filename, int* bytes) { return mem_map(filename, bytes, true); }

void mem_unmap(void* address, int bytes) {
    if (address != null) {
        int b = UnmapViewOfFile(address); (void)bytes; /* unused */
        assert(b); (void)b;
    }
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

static void toggle_full_screen(context_t* context, bool fs) {
    if (!context->full_screen) {
        // Chrome: Save current window information.  We force the window into restored mode
        // before going fullscreen because Windows doesn't seem to hide the
        // taskbar if the window is in the maximized state.
        context->full_screen_saved_maximized = !!::IsZoomed(context->window);
        if (context->full_screen_saved_maximized) { SendMessage(context->window, WM_SYSCOMMAND, SC_RESTORE, 0); }
        context->full_screen_saved_style    = GetWindowLong(context->window, GWL_STYLE);
        context->full_screen_saved_ex_style = GetWindowLong(context->window, GWL_EXSTYLE);
        GetWindowRect(context->window, &context->full_screen_saved_rect);
    }
    context->full_screen = fs;
    RECT* rc = null;
    if (fs) {
        SetWindowLong(context->window, GWL_STYLE, context->full_screen_saved_style & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLong(context->window, GWL_EXSTYLE,
            context->full_screen_saved_ex_style & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        MONITORINFO monitor_info = {};
        monitor_info.cbSize = sizeof(monitor_info);
        GetMonitorInfo(MonitorFromWindow(context->window, MONITOR_DEFAULTTONEAREST), &monitor_info);
        rc = &monitor_info.rcMonitor;
    } else {
        // Chrome: Reset original window style and size.  The multiple window size/moves
        // here are ugly, but if SetWindowPos() doesn't redraw, the taskbar won't be
        // repainted.  Better-looking methods are welcome.
        SetWindowLong(context->window, GWL_STYLE, context->full_screen_saved_style);
        SetWindowLong(context->window, GWL_EXSTYLE, context->full_screen_saved_ex_style);
        if (context->full_screen_saved_maximized) {
            ::SendMessage(context->window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        }
        rc = &context->full_screen_saved_rect;
    }
    SetWindowPos(context->window, null, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

static void activate_app(context_t* context) {
    if (context->app.visible && context->app.presentation != PRESENTATION_MINIMIZED) {
        SetForegroundWindow(context->window);
        bool a = context->app.active;
        context->app.active = true;
        if (a != context->app.active && context->app.changed != null) {
            context->app.changed(&context->app);
        }
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    context_t* context = (context_t*)GetPropA(window, "app.context");
    app_t* app = context != null ? &context->app : null;
    if (msg == WM_NCDESTROY) { return DefWindowProcA(window, msg, wp, lp); } // because GetWindowLongPtr won't work
    switch (msg) {
        case WM_GETMINMAXINFO: {
            MINMAXINFO* minmaxinfo = (MINMAXINFO*)lp;
            if (app->min_w > 0) { minmaxinfo->ptMinTrackSize.x = app->min_w; }
            if (app->min_h > 0) { minmaxinfo->ptMinTrackSize.y = app->min_h; }
            if (app->max_w > 0) { minmaxinfo->ptMaxTrackSize.x = app->max_w; }
            if (app->max_h > 0) { minmaxinfo->ptMaxTrackSize.y = app->max_h; }
            break;
        }
        case WM_CLOSE        : PostMessage(window, 0xC003, 0, 0);  return 0; // w/o calling DefWindowProc not to DestroyWindow() prematurely
        case 0xC003          :
            /* now all the messages has been processed in the queue */
            if (app->closing != null && app->closing(app)) {
                if (context->full_screen) { toggle_full_screen(context, false); }
                DestroyWindow(window);
            }
            return 0;
        case WM_DESTROY      : PostQuitMessage(0); break;
        case WM_CHAR         : app->keyboard(app, 0, 0, (int)wp); break;
        case WM_PAINT        : {
            PAINTSTRUCT ps = {};
            RECT rc;
            BeginPaint(window, &ps);
            SelectObject(ps.hdc, GetStockObject(NULL_BRUSH));
            GetClientRect(window, &rc);
            if (!wglMakeCurrent(ps.hdc, context->glrc)) {
                assertion(false, "%d %s", GetLastError(), strerr(GetLastError()));
            }
            app->paint(app, 0, 0, rc.right - rc.left, rc.bottom - rc.top);
            if (!SwapBuffers(ps.hdc)) {
                assertion(false, "%d %s", GetLastError(), strerr(GetLastError()));
            }
            EndPaint(window, &ps);
            break;
        }
        case WM_ACTIVATE:
            switch (LOWORD(wp)) {
                case WA_INACTIVE: app->active = false; break;
                case WA_CLICKACTIVE:
                case WA_ACTIVE: app->active = false; break;
                default: traceln("WARNING: new WM_ACTIVE state %d not handled", LOWORD(wp)); break;
            }
            if (!app->active && context->full_screen) { toggle_full_screen(context, false); }
            if (app->changed != null) { app->changed(app); }
            break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            activate_app(context);
            break;
        case WM_SETFOCUS:
            activate_app(context);
            break;
        case WM_ACTIVATEAPP: {
            if (wp) { activate_app(context); }
            break;
        }
        case WM_WINDOWPOSCHANGED: {
            WINDOWPOS* pos = (WINDOWPOS*)lp;
            if ((pos->flags & SWP_NOMOVE) == 0) {
                app->x = pos->x;
                app->y = pos->y;
            }
            if ((pos->flags & SWP_NOSIZE) == 0) {
                app->w = pos->cx;
                app->h = pos->cy;
            }
            if (IsIconic(context->window)) {
                app->presentation = PRESENTATION_MINIMIZED;
            } else if (IsZoomed(context->window)) {
                app->presentation = PRESENTATION_MAXIMIZED;
            } else  {
                app->presentation = context->full_screen ? PRESENTATION_FULL_SCREEN : PRESENTATION_NORMAL;
            }
            app->visible = IsWindowVisible(context->window);
            app->active = GetActiveWindow() == context->window;
            if (app->changed != null) { app->changed(app); }
            break;
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
            if (pt.y < 40) { return HTCAPTION; };
            break;
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
               CreateWindowExA(WS_EX_APPWINDOW, // WS_EX_COMPOSITED|WS_EX_LAYERED
                               wc.lpszClassName, "", WS_POPUP, // WS_POPUP to delay call to WM_GETMINMAXINFO
                               context->app.x, context->app.y, context->app.w, context->app.h,
                               null, null, GetModuleHandle(null), context);
    assert(atom == 0 || context->window != null);
    if (context->window == null) {
        ExitProcess(ERROR_FATAL_APP_EXIT);
    } else {
        SetPropA(context->window, "app.context", context);
    }
}

static DWORD WINAPI logger_thread(void* param) {
    SetThreadDescription(GetCurrentThread(), L"logger_thread");
    context_t* context = (context_t*)param;
    char text[2 * 1024 + 1]; // OutputDebugStringA used to have 2KB limitation
    while (!context->quiting) {
        DWORD available = 0;
        if (PeekNamedPipe(context->pipe_stdout_read, text, sizeof(text) - 1, null, &available, null) && available > 0) {
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

static void join_logger_thread(context_t* context) {
    context->quiting = true;
    byte wake_up_thread = 0;
    OVERLAPPED overlapped = {}; // to avoid WriteFile deadlock when thread is done before peeking and reading the pipe
    overlapped.hEvent = CreateEvent(null, false, false, null);
    WriteFile(context->pipe_stdout_write, &wake_up_thread, 1, null, &overlapped);
    WaitForSingleObject(context->logger_thread, INFINITE); // join thread
    CancelIoEx(context->pipe_stdout_write, &overlapped);
    CloseHandle(overlapped.hEvent);
}

static void redirect_std(FILE* std, int fd) {
    freopen("NUL", "wt", std);
    assert(_fileno(std) > 0);
    if (_dup2(fd, _fileno(std)) != 0) { assert(false); };
    setvbuf(std, null, _IONBF, 0);
}

static void redirect_io(context_t* context) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = false;
    bool b = CreatePipe(&context->pipe_stdout_read, &context->pipe_stdout_write, &sa, 0);
    assert(b);
    int fdbug = _open_osfhandle((intptr_t)context->pipe_stdout_write, _O_WRONLY | _O_TEXT);
    assert(fdbug > 0);
    stdbug = _fdopen(fdbug, "wt");
    setvbuf(stdbug, null, _IONBF, 0);
    assert(stdbug != null);
    if (!context->subsystem_console) { // do not redirect subsystem_console handle even if app is begging
        if (_fileno(stdout) < 0) {
            context->std_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
            SetStdHandle(STD_OUTPUT_HANDLE, context->pipe_stdout_write);
            redirect_std(stdout, fdbug);
        }
        if (_fileno(stderr) < 0) {
            context->std_err_handle = GetStdHandle(STD_ERROR_HANDLE);
            SetStdHandle(STD_ERROR_HANDLE, context->pipe_stdout_write);
            redirect_std(stderr, fdbug);
        }
    }
    context->logger_thread = CreateThread(null, 0, logger_thread, context, 0, null);
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

static void resize_to_console_window(context_t* context) {
    RECT rc = {};
    GetWindowRect(context->parent_console_window, &rc);
    context->app.x = rc.left;
    context->app.y = rc.top;
    context->app.w = rc.right - rc.left;
    context->app.h = rc.bottom - rc.top;
}

static void attach_console(context_t* context) {
    context->parrent_console_attached = AttachConsole(ATTACH_PARENT_PROCESS);
    if (context->parrent_console_attached) {
        freopen("CONOUT$", "wt", stdout);
        freopen("CONOUT$", "wt", stderr);
        freopen("NUL", "rt", stdin);
    }
    context->parent_console_window = GetConsoleWindow();
    if (context->parent_console_window != null) {
        resize_to_console_window(context);
        EnableWindow(context->parent_console_window, false);
        context->parent_console_input  = GetStdHandle(STD_INPUT_HANDLE);
        context->parent_console_output = GetStdHandle(STD_OUTPUT_HANDLE);
        context->parent_console_error  = GetStdHandle(STD_ERROR_HANDLE);
        SetStdHandle(STD_INPUT_HANDLE, (HANDLE)_get_osfhandle(_fileno(stdin)));
        CancelIo(context->parent_console_input);
        FlushConsoleInputBuffer(context->parent_console_input);
    }
}

static void hide_parent_console(context_t* context) {
    if (context->app.visible && context->parent_console_window != null && IsWindowVisible(context->parent_console_window)) {
        ShowWindow(context->parent_console_window, SW_HIDE);
    }
}

static void show_parent_console(context_t* context) {
    if (context->parent_console_window != null) {
        INPUT_RECORD ir[2] = {};
        ir[0].EventType = KEY_EVENT;
        ir[0].Event.KeyEvent.bKeyDown = 1;
        ir[0].Event.KeyEvent.wRepeatCount = 1;
        ir[0].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
        ir[0].Event.KeyEvent.wVirtualScanCode = VkKeyScanA('\r');
        ir[0].Event.KeyEvent.uChar.AsciiChar = '\r';
        ir[1] = ir[0];
        ir[0].Event.KeyEvent.bKeyDown = 0;
        DWORD dwWritten = 0;
        EnableWindow(context->parent_console_window, true);
        WriteConsoleInputA(context->parent_console_input, ir, 2, &dwWritten);
        ShowWindow(context->parent_console_window, SW_RESTORE);
        SetFocus(context->parent_console_window);
        CONSOLE_CURSOR_INFO ci = { sizeof(CONSOLE_CURSOR_INFO) };
        GetConsoleCursorInfo(context->parent_console_output, &ci);
        ci.bVisible = true;
        SetConsoleCursorInfo(context->parent_console_output, &ci);
        SetForegroundWindow(context->parent_console_window);
    }
}

static void show_window(context_t* context, int show_command) {
    static const int color_bits = 32;
    static const int alpha_bits = 0;
    static const int depth_bits = 24;
    static const int stencil_bits = 8;
    static const int accum_bits = 0;
    HDC dc = GetDC(context->window);
    bool b = set_pixel_format(context->window, color_bits, alpha_bits, depth_bits, stencil_bits, accum_bits);
    context->glrc = b ? wglCreateContext(dc) : null;
    if (b && context->glrc != null) { b = wglMakeCurrent(dc, context->glrc); }
    ReleaseDC(context->window, dc);
    assertion(b, "wglCreateContext() failed: %s", strerr(GetLastError()));
    if (!b) { ExitProcess(0x1BADF00D); }
    context->app.begin(&context->app); // now visibility and min max info can be used
    SetWindowTextA(context->window, context->app.title != null ? context->app.title : "");
    DWORD style = GetWindowLong(context->window, GWL_STYLE);
    SetWindowLong(context->window, GWL_STYLE, (style & ~(WS_POPUP)) | WS_OVERLAPPED);
    ShowWindow(context->window, show_command);
    hide_parent_console(context);
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

static void app_asset(app_t* a, const char* name, void* *data, int *bytes) {
    (void)a;
    const HMODULE module = GetModuleHandle(null);
    const HRSRC  res = FindResourceA(module, name, RT_RCDATA);
    const HANDLE handle = res != null ? LoadResource(module, res) : null;
    *data = handle != null ? LockResource(handle) : null;
    *bytes = handle != null ? (int)SizeofResource(module, res) : 0;
}

static void app_quit(app_t* a, int exit_code) { (void)a; PostQuitMessage(exit_code); }
static void app_exit(app_t* a, int exit_code) { (void)a; exit(exit_code); }
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

static void app_notify(app_t* a) {
    context_t* context = (context_t*)a;
    RECT rc;
    GetClientRect(context->window, &rc);
    int x = rc.left;
    int y = rc.top;
    int w = rc.right - x;
    int h = rc.bottom - y;
    HWND top = GetTopWindow(null);
    bool topmost = top == context->window;
    bool active = GetActiveWindow() == context->window;
    bool visible = IsWindowVisible(context->window);
    bool iconic = IsIconic(context->window);
    bool zoomed = IsZoomed(context->window);
    const int  presentation = a->presentation;
    bool minimized = presentation == PRESENTATION_MINIMIZED;
    bool maximized = presentation == PRESENTATION_MAXIMIZED;
    bool full_screen = presentation == PRESENTATION_FULL_SCREEN;
    // not used: SWP_ASYNCWINDOWPOS, SWP_DEFERERASE, SWP_NOSENDCHANGING, SWP_FRAMECHANGED, SWP_NOCOPYBITS
    const DWORD none = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    DWORD flags = none;
    if (a->visible != visible) {
        flags |= a->visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
    }
    if (a->active != active && a->active) { flags &= ~SWP_NOACTIVATE; }
    if (a->topmost != topmost && a->topmost) { flags &= ~(SWP_NOOWNERZORDER | SWP_NOZORDER); }
    if (a->x != x || a->y != y) { flags &= ~SWP_NOMOVE; }
    if (a->w != w || a->h != h) { flags &= ~SWP_NOSIZE; }
    if (flags != none) {
        SetWindowPos(context->window, a->topmost ? top : null, a->x, a->y, a->w, a->h, flags);
    }
    if (a->visible) { // do not mess with Maximize, Minimize in invisible state. Window Manager will behave strangely. Bugs in MS Windows
        if (context->full_screen != full_screen) {
            if (!full_screen) { context->full_screen_saved_presentation = presentation; }
            toggle_full_screen(context, full_screen);
            a->presentation = context->full_screen ? PRESENTATION_FULL_SCREEN : context->full_screen_saved_presentation;
        } else {
            if (minimized != iconic) { SendMessageA(context->window, WM_SYSCOMMAND, iconic ? SC_RESTORE : SC_MINIMIZE, 0); }
            if (maximized != zoomed) { SendMessageA(context->window, WM_SYSCOMMAND, zoomed ? SC_RESTORE : SC_MAXIMIZE, 0); }
        }
    }
    SetWindowText(context->window, a->title != null ? a->title : "");
    hide_parent_console(context);
}

int app_run(void (*init)(app_t* app), int show_command, int argc, const char** argv) {
    context_t context = {};
    app_t* app = &context.app;
    app->quit  = app_quit;
    app->exit  = app_exit;
    app->abort = app_abort;
    app->asset = app_asset;
    app->toast = app_toast;
    app->notify = app_notify;
    app->message_box = app_message_box;
    app->invalidate = app_invalidate;
    app->invalidate_rectangle = app_invalidate_rectangle;
    context.subsystem_console = argc > 0 && argv != null;
    if (!context.subsystem_console) {
        const char* cl = GetCommandLineA();
        const int len = (int)strlen(cl);
        const int k = ((len + 2) / 2) * sizeof(void*) + sizeof(void*);
        const int n = k + (len + 2) * sizeof(char);
        app->argv = (const char**)alloca(n);
        memset(app->argv, 0, n);
        char* buff = (char*)(((char*)app->argv) + k);
        app->argc = parse_argv(cl, app->argv, buff);
    } else {
        app->argc = argc;
        app->argv = argv;
    }
    attach_console(&context);
    redirect_io(&context);
    init(app);
    PostThreadMessage(GetCurrentThreadId(), 0xC001, 0, 0);
    MSG msg = {};
    while (GetMessage(&msg, null, 0, 0)) {
        if (msg.message == 0xC001) {
            create_window(&context);
        } else if (msg.message == 0xC002) {
            show_window(&context, show_command);
        } else {
            TranslateMessage(&msg); // WM_KEYDOWN/UP -> WM_CHAR, WM_CLICK, WM_CLICK -> WM_DBLCLICK ...
            DispatchMessage(&msg);
        }
    }
    app->end(app);
    show_parent_console(&context);
    join_logger_thread(&context);
    fclose(stdbug); // will actually close context.pipe_stdout_read
    CloseHandle(context.pipe_stdout_read);
    CloseHandle(context.logger_thread);
    // no need to fclose: stdout and stderr because both of them are
    // attached to the same pipe_stdout_write handle which has been closed
    if (context.std_out_handle != null) { SetStdHandle(STD_OUTPUT_HANDLE, context.std_out_handle); }
    if (context.std_out_handle != null) { SetStdHandle(STD_ERROR_HANDLE, context.std_err_handle); }
    assert(msg.message == WM_QUIT);
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

