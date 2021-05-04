#include "platform_wm.hpp"

void wm_init(WMContext &wm) {
    wm.display = XOpenDisplay(nullptr);
    wm.window = XCreateSimpleWindow(wm.display,
                                    XDefaultRootWindow(wm.display),
                                    0, 0,
                                    640, 480,
                                    0, 0,
                                    XBlackPixel(wm.display, XDefaultScreen(wm.display)));
}

