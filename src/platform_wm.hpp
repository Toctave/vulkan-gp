#pragma once

#if 1

#include "x11/wm.hpp"
using WMContext = X11Context;

#endif

void wm_init(WMContext& wm);
void wm_finalize(WMContext& wm);

