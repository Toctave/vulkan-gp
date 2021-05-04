#pragma once

#include <X11/Xlib.h>

struct X11Context {
    Display* display;
    Window window;
};
