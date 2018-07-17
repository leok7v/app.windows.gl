#pragma once

#include "std.h"
#include "ogl.h"
#include "mem.h"
#include "app.h"
#if defined(_WINDOWS)
#include "app_windows.h"
#elif defined(_OSX)
#include "app_windows.h"
#elif  defined(_ANDROID)
#include "app_android.h"
#endif

BEGIN_C

#if defined(_WINDOWS)

static void init(app_t* a) { app_init(a); }

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ char*, _In_ int show_command) {
    app_t app = {};
    switch (show_command) {
        case SW_HIDE         : app.visibility = WINDOW_HIDE; break;
        case SW_SHOWDEFAULT  : app.visibility = WINDOW_SHOW; break;
        case SW_SHOWNORMAL   : app.visibility = WINDOW_SHOW; break;
        case SW_SHOWMINIMIZED: app.visibility = WINDOW_MIN;  break;
        case SW_SHOWMAXIMIZED: app.visibility = WINDOW_MAX;  break;
        default: traceln("unexpected show_command=%d defaulted to WINDOW_SHOW", show_command);
                               app.visibility = WINDOW_SHOW; break;
    }
    return (int)app_run(init); // argc = 0 and argv = null for WinMain /SUBSYSTEM:WINDOWS
}

int main(int argc, const char** argv) {
    app_t app = {};
    app.argc = argc;
    app.argv = argv;
    app_init(&app);
    return app_run(init); // argc >= 1 and argv != null && argv[0] != null /SUBSYSTEM:CONSOLE
}

#elif defined(_OSX)

int main(int argc, char* argv[]) {
    return 0; // TODO: implement
}

#elif  defined(_ANDROID)

// TODO: implement me

#endif

END_C

