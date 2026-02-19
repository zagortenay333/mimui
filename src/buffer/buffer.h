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

istruct (Buf) {
    Mem *mem;
    ArrayChar data;
    ArrayString lines;
    U32 widest_line;
    Bool dirty;
};

Void compute_aux(Buf *buf);
Buf         *buf_new                      (Mem *, String);
Buf         *buf_new_from_file            (Mem *, String filepath);
BufLineIter *buf_line_iter_new            (Buf *, Mem *, U32);
Bool         buf_line_iter_next           (BufLineIter *);
Void         buf_clear                    (Buf *);
Buf         *buf_copy                     (Buf *, Mem *);
String       buf_get_line                 (Buf *, Mem *, U32);
U32          buf_get_widest_line          (Buf *);
U32          buf_get_line_count           (Buf *);
Void         buf_insert_                  (Buf *, U64 offset, String str);
Void         buf_delete_                  (Buf *, U64 offset, U64 count);
U32          buf_get_count                (Buf *);
String       buf_get_str                  (Buf *, Mem *);

#define buf_iter_lines(IT, BUF, MEM, FROM)\
    for (BufLineIter *IT = buf_line_iter_new(BUF, MEM, FROM); !IT->done; buf_line_iter_next(IT))
