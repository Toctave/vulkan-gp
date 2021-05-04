#pragma once

#if 1

#include "platform_x11.hpp"
using WMContext = X11Context;

#endif

void wm_init(WMContext& wm);
void wm_finalize(WMContext& wm);

