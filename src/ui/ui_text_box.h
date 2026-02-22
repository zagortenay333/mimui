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

istruct (UiTextBoxVisualLine) {
    U64 logical_line_offset; // Byte offset of containing logical line.
    U64 logical_line_count; // Byte length of logical line.
    U64 logical_col; // Column in logical line counting codepoints.
    U64 offset; // Byte offset of visual line.
    U64 count; // Byte length of visual line.
};

istruct (UiTextBoxCursor) {
    U64 byte_offset;
    U64 selection_offset;
    U64 line; // Index into visual_lines.
    U64 column; // 0-indexed and counting codepoints not bytes.
    U64 preferred_column;
};

istruct (UiTextBoxInfo) {
    Mem *mem;
    Buf *buf;
    U64 buf_version;
    UiTextBoxCursor cursor;
    Vec2 cursor_coord;
    Vec2 scroll_coord;
    Vec2 scroll_coord_n;
    F32 total_width;
    F32 total_height;
    Bool dragging;
    Bool single_line_mode;
    Bool dirty;
    UiTextBoxWrapMode wrap_mode;
    Array(UiTextBoxVisualLine) visual_lines;
    U64 widest_line;
    U64 viewport_width;
    U64 char_width;
    U64 tab_width;
};

UiBox *ui_tbox                           (String id, Buf *buf, Bool single_line_mode, UiTextBoxWrapMode);
Void   ui_tbox_cursor_move_left          (UiTextBoxInfo *, UiTextBoxCursor *, Bool move_selection);
Void   ui_tbox_cursor_move_right         (UiTextBoxInfo *, UiTextBoxCursor *, Bool move_selection);
Void   ui_tbox_cursor_move_up            (UiTextBoxInfo *, UiTextBoxCursor *, Bool move_selection);
Void   ui_tbox_cursor_move_left_word     (UiTextBoxInfo *, UiTextBoxCursor *, Bool move_selection);
Void   ui_tbox_cursor_move_right_word    (UiTextBoxInfo *, UiTextBoxCursor *, Bool move_selection);
Void   ui_tbox_cursor_move_down          (UiTextBoxInfo *, UiTextBoxCursor *, Bool move_selection);
Void   ui_tbox_cursor_move_to_start      (UiTextBoxInfo *, UiTextBoxCursor *, Bool move_selection);
Void   ui_tbox_cursor_move_to_end        (UiTextBoxInfo *, UiTextBoxCursor *, Bool move_selection);
Void   ui_tbox_cursor_clamp              (UiTextBoxInfo *, UiTextBoxCursor *);
String ui_tbox_cursor_get_selection      (UiTextBoxInfo *, Mem *, UiTextBoxCursor *);
U64    ui_tbox_cursor_line_col_to_offset (UiTextBoxInfo *, UiTextBoxCursor *);
Void   ui_tbox_cursor_offset_to_line_col (UiTextBoxInfo *, UiTextBoxCursor *);
Void   ui_tbox_cursor_swap_offset        (UiTextBoxInfo *, UiTextBoxCursor *);
Void   ui_tbox_cursor_delete             (UiTextBoxInfo *, UiTextBoxCursor *);
Void   ui_tbox_cursor_insert             (UiTextBoxInfo *, UiTextBoxCursor *, String);
