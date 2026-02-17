#pragma once

#include "base/core.h"

Void ui_init         ();
Void ui_frame        (Void(*)(), F64 dt);
Bool ui_is_animating ();
