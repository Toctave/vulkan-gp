#include "wm.hpp"

void wm_init(X11Context &wm) {
    wm.display = XOpenDisplay(nullptr);
    wm.window = XCreateSimpleWindow(wm.display,
                                    XDefaultRootWindow(wm.display),
                                    0, 0,
                                    640, 480,
                                    0, 0,
                                    XBlackPixel(wm.display, XDefaultScreen(wm.display)));
}

void wm_finalize(X11Context& wm) {
    // XDestroyWindow(wm.display, wm.window);
    XCloseDisplay(wm.display);
}
