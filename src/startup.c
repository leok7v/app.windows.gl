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

int WINAPI WinMain(HINSTANCE, HINSTANCE, char*, int show_command) {
    // argc = 0 and argv = null for WinMain /SUBSYSTEM:WINDOWS
    return (int)app_run(init, show_command, 0, null);
}

int main(int argc, const char** argv) {
    return (int)app_run(init, SW_RESTORE, argc, argv); // argc >= 1 and argv != null && argv[0] != null /SUBSYSTEM:CONSOLE
}

#elif defined(_OSX)

int main(int argc, char* argv[]) {
    return 0; // TODO: implement
}

#elif  defined(_ANDROID)

// TODO: implement me

#endif

END_C

