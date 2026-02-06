#include "buffer/buffer.h"
#include "os/fs.h"
#include "os/time.h"

istruct (Buf) {
    Mem *mem;
    ArrayChar data;
    ArrayString lines;
    U64 widest_line;
    Bool dirty;
};

static Void compute_aux (Buf *buf) {
    if (! buf->dirty) return;
    buf->dirty = false;
    str_split(buf->data.as_slice, str("\n"), 0, 1, &buf->lines);
    buf->lines.count--;
    array_iter (line, &buf->lines, *) {
        U64 count = str_codepoint_count(*line);
        if (count > buf->widest_line) buf->widest_line = count;
    }
}

Buf *buf_new (Mem *mem, String text) {
    Auto buf   = mem_new(mem, Buf);
    buf->mem   = mem;
    buf->dirty = true;
    buf_insert(buf, text, 0);
    array_init(&buf->data, mem);
    array_init(&buf->lines, mem);
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

BufLineIter *buf_line_iter_new (Buf *buf, Mem *mem, U64 from) {
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

String buf_get_line (Buf *buf, Mem *mem, U64 idx) {
    compute_aux(buf);
    idx = clamp(idx, 0u, buf->lines.count-1);
    return array_get(&buf->lines, idx);
}

U64 buf_get_widest_line (Buf *buf) {
    compute_aux(buf);
    return buf->widest_line;
}

U64 buf_get_line_count (Buf *buf) {
    compute_aux(buf);
    return buf->lines.count;
}

Void buf_insert (Buf *buf, String str, U64 idx) {
    array_insert_many(&buf->data, &str, idx);
    buf->dirty = true;
}

Void buf_delete (Buf *buf, U64 count, U64 idx) {
    count = clamp(count, 0u, buf->data.count - idx);
    array_remove_many(&buf->data, idx, count);
    buf->dirty = true;
}

U64 buf_get_count (Buf *buf) {
    return buf->data.count;
}

String buf_get_str (Buf *buf, Mem *) {
    return buf->data.as_slice;
}

U64 buf_line_to_offset (Buf *buf, U64 line_idx) {
    compute_aux(buf);
    line_idx = clamp(line_idx, 0u, buf->lines.count-1);
    String line = array_get(&buf->lines, line_idx);
    return line.data - buf->data.data;
}
