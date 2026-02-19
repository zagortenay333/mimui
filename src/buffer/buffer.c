#include "buffer/buffer.h"
#include "os/fs.h"
#include "os/time.h"

Void compute_aux (Buf *buf) {
    if (! buf->dirty) return;
    buf->dirty = false;

    buf->lines.count = 0;
    buf->widest_line = 0;
    U32 prev_pos = 0;

    array_iter (c, &buf->data) {
        if (c != '\n') continue;
        String s = str_slice(buf->data.as_slice, prev_pos, ARRAY_IDX - prev_pos);
        if (s.count > buf->widest_line) buf->widest_line = s.count;
        array_push(&buf->lines, s);
        prev_pos = ARRAY_IDX + 1;
    }

    if (prev_pos < buf->data.count) {
        String s = str_slice(buf->data.as_slice, prev_pos, buf->data.count - prev_pos);
        if (s.count > buf->widest_line) buf->widest_line = s.count;
        array_push(&buf->lines, s);
    }
}

Buf *buf_new (Mem *mem, String text) {
    Auto buf   = mem_new(mem, Buf);
    buf->mem   = mem;
    buf->dirty = true;
    array_init(&buf->data, mem);
    array_init(&buf->lines, mem);
    buf_insert_(buf, 0, text);
    return buf;
}

Buf *buf_new_from_file (Mem *mem, String filepath) {
    Auto buf = mem_new(mem, Buf);
    buf->mem = mem;
    buf->dirty = true;
    array_init(&buf->lines, mem);

    String file = fs_read_entire_file(mem, filepath, 1*KB);
    buf->data.mem = mem;
    buf->data.data = file.data;
    buf->data.count = file.count;
    buf->data.capacity = file.count + 1*KB + 1;

    return buf;
}

BufLineIter *buf_line_iter_new (Buf *buf, Mem *mem, U32 from) {
    compute_aux(buf);
    Auto it = mem_new(mem, BufLineIter);
    it->buf = buf;
    if (buf->lines.count) {
        from = clamp(from, 0u, buf->lines.count-1);
        it->idx = from;
        it->text = array_get(&buf->lines, from);
    } else {
        it->done = true;
    }
    return it;
}

Bool buf_line_iter_next (BufLineIter *it) {
    if (it->done) return true;
    it->idx++;
    if (it->idx >= it->buf->lines.count) {
        it->done = true;
    } else {
        it->text = array_get(&it->buf->lines, it->idx);
    }
    return it->done;
}

String buf_get_line (Buf *buf, Mem *mem, U32 idx) {
    compute_aux(buf);
    idx = clamp(idx, 0u, buf->lines.count - 1);
    return buf->lines.count ? array_get(&buf->lines, idx) : (String){};
}

U32 buf_get_widest_line (Buf *buf) {
    compute_aux(buf);
    return buf->widest_line;
}

U32 buf_get_line_count (Buf *buf) {
    compute_aux(buf);
    return buf->lines.count;
}

Void buf_insert_ (Buf *buf, U64 offset, String str) {
    array_insert_many(&buf->data, &str, offset);
    buf->dirty = true;
}

Void buf_delete_ (Buf *buf, U64 offset, U64 count) {
    array_remove_many(&buf->data, offset, count);
    buf->dirty = true;
}

U32 buf_get_count (Buf *buf) {
    return buf->data.count;
}

String buf_get_str (Buf *buf, Mem *) {
    return buf->data.as_slice;
}
Void buf_clear (Buf *buf) {
    buf->data.count = 0;
    buf->dirty = true;
}

Buf *buf_copy (Buf *buf, Mem *mem) {
    return buf_new(mem, buf->data.as_slice);
}
