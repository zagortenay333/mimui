#pragma once

#include "base/core.h"
#include "base/mem.h"
#include "base/string.h"

istruct (Buffer) {
    Mem *mem;
    String text;
    ArrayString lines;
    U64 widest_line;
};

istruct (BufferLineIter) {
    Buffer *buf;
    U64 idx;
    String text;
};

Buffer         *buf_new             (Mem *, String);
BufferLineIter *buf_line_iter_new   (Buffer *, Mem *);
Bool            buf_line_iter_next  (BufferLineIter *);
String          buf_get_line        (Buffer *, Mem *, U64);
U64             buf_get_widest_line (Buffer *);

#define buf_iter_lines(BUF, MEM)\
    for (BufferLineIter *it = buffer_line_iter_new(BUF, MEM); bufer_line_iter_next(it);)
