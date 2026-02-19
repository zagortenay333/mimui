#pragma once

#include "ui/ui.h"
#include "base/core.h"
#include "buffer/buffer.h"
#include "base/string.h"

ienum (UiTextBoxWrapMode, U8) {
    LINE_WRAP_NONE,
    LINE_WRAP_CHAR,
    LINE_WRAP_WORD,
};

UiBox *ui_text_box (String id, Buf *buf, Bool single_line_mode, UiTextBoxWrapMode);
