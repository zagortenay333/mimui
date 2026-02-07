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
    Buf *buf;
    U64 byte_offset;
    U64 selection_offset;
    U64 line; // 0-indexed
    U64 column; // 0-indexed and counting codepoints not bytes.
    U64 preferred_column;
};

Buf         *buf_new                (Mem *, String);
Buf         *buf_new_from_file      (Mem *, String filepath);
BufLineIter *buf_line_iter_new      (Buf *, Mem *, U64);
Bool         buf_line_iter_next     (BufLineIter *);
String       buf_get_line           (Buf *, Mem *, U64);
U64          buf_get_widest_line    (Buf *);
U64          buf_get_line_count     (Buf *);
Void         buf_insert             (Buf *, String str, U64 idx);
Void         buf_delete             (Buf *, U64 count, U64 idx);
U64          buf_get_count          (Buf *);
String       buf_get_str            (Buf *, Mem *);
U64          buf_line_col_to_offset (Buf *, U64 line, U64 column);
BufCursor    buf_cursor_new         (Buf *, U64 line, U64 column);
Void         buf_cursor_move_left   (BufCursor *);
Void         buf_cursor_move_right  (BufCursor *);
Void         buf_cursor_move_up     (BufCursor *);
Void         buf_cursor_move_down   (BufCursor *);

#define buf_iter_lines(IT, BUF, MEM, FROM)\
    for (BufLineIter *IT = buf_line_iter_new(BUF, MEM, FROM); !IT->done; buf_line_iter_next(IT))
