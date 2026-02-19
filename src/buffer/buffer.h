#pragma once

#include "base/core.h"
#include "base/mem.h"
#include "base/string.h"

istruct (Buf);

istruct (BufLineIter) {
    Buf *buf;
    U64 idx;
    U64 offset;
    String text;
    Bool done;
};

Buf         *buf_new               (Mem *, String);
Buf         *buf_new_from_file     (Mem *, String filepath);
BufLineIter *buf_line_iter_new     (Buf *, Mem *, U32);
Bool         buf_line_iter_next    (BufLineIter *);
String       buf_get_range         (Buf *, U64 offset, U64 count); // @todo
Void         buf_clear             (Buf *);
Void         buf_insert            (Buf *, U64 offset, String str);
Void         buf_delete            (Buf *, U64 offset, U64 count);
U32          buf_get_count         (Buf *);
String       buf_get_str           (Buf *, Mem *);
Bool         buf_ends_with_newline (Buf *);
U64          buf_find_prev_word    (Buf *, U64 from);
U64          buf_find_next_word    (Buf *, U64 from);

#define buf_iter_lines(IT, BUF, MEM, FROM)\
    for (BufLineIter *IT = buf_line_iter_new(BUF, MEM, FROM); !IT->done; buf_line_iter_next(IT))
