#pragma once
#include "std.h"

BEGIN_C

typedef struct app_s app_t;

enum {
    VISIBILITY_HIDE = 1, // window will become invisible
    VISIBILITY_SHOW = 2, // restore normal window at (x, y, w, h) position
    VISIBILITY_MAX  = 3, // maximize window to the size of the nearest monitor
    VISIBILITY_MIN  = 4, // window will become minimized
    VISIBILITY_FULL = 5  // full screen window on the nearest monitor
};

enum {                              // result:
    MESSAGE_BOX_OK            = 0, 
    MESSAGE_BOX_OK_CANCEL     = 1,  // OK  1 CANCEL -1
    MESSAGE_BOX_YES_NO        = 4,  // YES 1 NO 0
    MESSAGE_BOX_YES_NO_CANCEL = 3,  // YES 1 NO 0 CANCEL -1
    MESSAGE_BOX_ICON_ERROR    = 0x00000010,
    MESSAGE_BOX_ICON_QUESTION = 0x00000020,
    MESSAGE_BOX_ICON_WARNING  = 0x00000030,
    MESSAGE_BOX_ICON_INFO     = 0x00000040,
};

typedef struct app_s {
    // application callbacks:
    void (*begin)(app_t* a); // called after platform created application window
    void (*changed)(app_t* a, int x, int y, int w, int h, int v); // user or window manager changed geometry or visibility
    void (*paint)(app_t* a, int x, int y, int w, int h);
    void (*keyboard)(app_t* a, int state, int key, int character);
    // index of multitouch 'finger' (index == 0 means mouse to trackpad)
    void (*touch)(app_t* a, int state, int index, int x, int y, float pressure, float proximity);
    bool (*closing)(app_t* a); // user or window manager attempting to close a window
    void (*end)(app_t* a);     // called before application exit
    // this is platform defined actions only valid between begin() and end() calls:
    void (*toast)(app_t* a, int seconds, const char* format, ...);
    int  (*message_box)(app_t* a, int flags, const char* format, ...);
    void (*invalidate)(app_t* a);
    void (*invalidate_rectangle)(app_t* app, int x, int y, int w, int h);
    void (*quit)(app_t* a, int exit_code);  // posts quit message to application main dispatch queue
    void (*exit)(app_t* a, int exit_code);  // exits process immediately executing atexit() handlers
    void (*abort)(app_t* a, int exit_code); // aborts process execution possibly skipping atexit() handlers
    void (*asset)(app_t* a, const char* name, void* *data, int *bytes);
    // supplied by startup code valid only between begin() and end() callbacks:
    int argc;
    const char** argv;
    const char*  title;
    // following values are examined by platform on each main even queue dispatch 
    int visibility; 
    int x;
    int y;
    int w;     
    int h;
    int min_w; 
    int min_h;
    int max_w;
    int max_h;
} app_t;

const char* strerr(int r); // extended platform specific strerror()
void app_init(app_t* app); // implemented by application code called by startup
// implemented by platform code called by startup code:
int  app_run(void (*init)(app_t* app), int argc, const char** argv, int visibility);

END_C

/*
    "stdbug" and traceln()
    in addition to stdout and stderr stdbug handle writes log (trace) messages to 
    the system log facility. On OSX it is same as stderr. On Windows it's different
    see below. On Android it's logcat

    Windows specifics:
    Platform allow UI apps to be started as /SUBSYTEM:WINDOWS and /SUBSYSTEM:CONSOLE
    On Windows UI subsystem application usually starts with stderr and stdout as 
    closed handles (_fileno(std*) == -2. If and only if this is the case
    both are "redirected" to OutputDebugString in case code you are running is
    using printf() and fprintf().
    If you do not want to spam the debug log you can close both files by redirecting
    them to "NUL" device: reopen("NUL", "wt", stdout); reopen("NUL", "wt", stderr);
    Because they share the handle with stdbug fclose(stdout); fclose(stderr); will
    have unexpected result - first one will close all three and second one will raise
    0xC0000008 invalid handle exception inside NTDLL.
    If you run UI app.exe >foo 2>bar the handles will NOT be redirected to stdbug.
*/