#pragma once
#include "std.h"

BEGIN_C

extern FILE* stdtrace; // implemented by platform can be /dev/null

#define traceln(format, ...) fprintf(stdtrace, "%s(%d): %s" format "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef DEBUG
#undef assert 
#define assertion(b, format, ...) (void)( (!!(b)) || (fprintf(stderr, "%s(%d): %s assertion " ## #b ## "failed. " format "\n", __FILE__, __LINE__, __func__, __VA_ARGS__), 0)) 
#define assert(b) assertion(b, "") 
#else
#undef assert 
#define assertion(b, format, ...) (void)(0)
#define assert(b) (void)(0)
#endif

typedef struct app_s app_t;

enum {
    WINDOW_HIDE = 1, // window will become invisible
    WINDOW_SHOW = 2, // restore normal window at (x, y, w, h) position
    WINDOW_MAX  = 3, // maximize window to the size of the nearest monitor
    WINDOW_MIN  = 4, // window will become minimized
    WINDOW_FULL = 5  // full screen window on the nearest monitor
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
    bool redirect_std; // true -> stdout and stderr also redirected to logd or OutputDebugStringA
} app_t;

const char* strerr(int r); // extended platform specific strerror()
void app_init(app_t* app); // implemented by application code called by startup
// implemented by platform code called by startup code:
int  app_run(void (*init)(app_t* app), int argc, const char** argv, int visibility);

END_C
