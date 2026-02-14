#pragma once

#include "base/core.h"
#include "base/mem.h"
#include "base/string.h"

istruct (Buf);

istruct (BufLineIter) {
    Buf *buf;
    U64 idx;
    String text;
    Bool done;
};

istruct (BufCursor) {
    U32 byte_offset;
    U32 selection_offset;
    U32 line; // 0-indexed
    U32 column; // 0-indexed and counting codepoints not bytes.
    U32 preferred_column;
};

Buf         *buf_new                      (Mem *, String);
Buf         *buf_new_from_file            (Mem *, String filepath);
BufLineIter *buf_line_iter_new            (Buf *, Mem *, U32);
Bool         buf_line_iter_next           (BufLineIter *);
Void         buf_clear                    (Buf *);
Buf         *buf_copy                     (Buf *, Mem *);
String       buf_get_line                 (Buf *, Mem *, U32);
U32          buf_get_widest_line          (Buf *);
U32          buf_get_line_count           (Buf *);
Void         buf_insert                   (Buf *, BufCursor *, String str);
Void         buf_delete                   (Buf *, BufCursor *);
U32          buf_get_count                (Buf *);
String       buf_get_str                  (Buf *, Mem *);
U32          buf_line_col_to_offset       (Buf *, U32 line, U32 column);
Void         buf_offset_to_line_col       (Buf *, BufCursor *);
BufCursor    buf_cursor_new               (Buf *, U32 line, U32 column);
Void         buf_cursor_swap_offset       (Buf *, BufCursor *);
Void         buf_cursor_move_left         (Buf *, BufCursor *, Bool);
Void         buf_cursor_move_right        (Buf *, BufCursor *, Bool);
Void         buf_cursor_move_left_word    (Buf *, BufCursor *, Bool);
Void         buf_cursor_move_right_word   (Buf *, BufCursor *, Bool);
Void         buf_cursor_move_up           (Buf *, BufCursor *, Bool);
Void         buf_cursor_move_down         (Buf *, BufCursor *, Bool);
Void         buf_cursor_move_to_start     (Buf *, BufCursor *, Bool);
Void         buf_cursor_move_to_end       (Buf *, BufCursor *, Bool);
Bool         buf_cursor_at_end_no_newline (Buf *, BufCursor *);
Void         buf_cursor_clamp             (Buf *, BufCursor *);
String       buf_get_selection            (Buf *, BufCursor *);

#define buf_iter_lines(IT, BUF, MEM, FROM)\
    for (BufLineIter *IT = buf_line_iter_new(BUF, MEM, FROM); !IT->done; buf_line_iter_next(IT))
