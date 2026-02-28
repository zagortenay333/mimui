#pragma once

#include "base/core.h"
#include "base/mem.h"
#include "base/string.h"
#include "ui/ui.h"

UiBox *ui_text_view                 (UiBoxFlags flags, String id, String text);
U64    ui_text_view_coord_to_offset (UiBox *box, Vec2 coord);
