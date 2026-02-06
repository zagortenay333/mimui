#pragma once

#include "base/core.h"
#include "base/mem.h"
#include "base/string.h"

istruct (Buf) {
    Mem *mem;
    U64 line_count;
    U64 widest_line;
    Bool stats_outdated;
    AString str;
    U64 gap_min;
    U64 gap_idx;
    U64 gap_count;
};

istruct (BufLineIter) {
    Buf *buf;
    U64 idx;
    String text;
    Bool done;
};

Buf         *buf_new             (Mem *, String);
Buf         *buf_new_from_file   (Mem *, String filepath);
BufLineIter *buf_line_iter_new   (Buf *, Mem *, U64);
Bool         buf_line_iter_next  (BufLineIter *);
String       buf_get_line        (Buf *, Mem *, U64);
U64          buf_get_widest_line (Buf *);
U64          buf_get_line_count  (Buf *);
Void         buf_insert          (Buf *, String str, U64 idx);
Void         buf_delete          (Buf *, U64 count, U64 idx);
U64          buf_get_count       (Buf *);
String       buf_get_str         (Buf *, Mem *);
U64          buf_line_to_offset  (Buf *, U64 line);
U64          buf_get_offset      (Buf *, U64 line, U64 column);

#define buf_iter_lines(IT, BUF, MEM, FROM)\
    for (BufLineIter *IT = buf_line_iter_new(BUF, MEM, FROM); !IT->done; buf_line_iter_next(IT))
