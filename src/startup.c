#pragma once

#include "std.h"
#include "ogl.h"
#include "mem.h"
#include "app.h"
#if defined(_WINDOWS)
#include "mswin.h"
#elif defined(_OSX)
#include "osx.h"
#elif  defined(_ANDROID)
#include "droid.h"
#endif

BEGIN_C

#if defined(_WINDOWS)

static void init(app_t* a) { app_init(a); }

static int show_command_to_visibility(int show_command) {
    switch (show_command) {
        case SW_HIDE         : return VISIBILITY_HIDE;
        case SW_SHOWDEFAULT  : return VISIBILITY_SHOW;
        case SW_SHOWNORMAL   : return VISIBILITY_SHOW;
        case SW_SHOWMINIMIZED: return VISIBILITY_MIN; 
        case SW_SHOWMAXIMIZED: return VISIBILITY_MAX; 
        default: traceln("unexpected show_command=%d defaulted to WINDOW_SHOW", show_command);
            return VISIBILITY_SHOW;
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, char*, int show_command) {
    // argc = 0 and argv = null for WinMain /SUBSYSTEM:WINDOWS
    return (int)app_run(init, 0, null, show_command_to_visibility(show_command));
}

int main(int argc, const char** argv) {
    return (int)app_run(init, argc, argv, VISIBILITY_SHOW); // argc >= 1 and argv != null && argv[0] != null /SUBSYSTEM:CONSOLE
}

#elif defined(_OSX)

int main(int argc, char* argv[]) {
    return 0; // TODO: implement
}

#elif  defined(_ANDROID)

// TODO: implement me

#endif

END_C

