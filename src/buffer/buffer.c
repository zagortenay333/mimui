#include "buffer/buffer.h"

static Void compute_lines (Buffer *buf) {
    buf->lines.count = 0;
    str_split(buf->text, str("\n"), 0, 1, &buf->lines);
    array_iter (line, &buf->lines) {
        if (line.count > buf->widest_line) buf->widest_line = line.count;
    }
}

Buffer *buf_new (Mem *mem, String text) {
    Auto buf = mem_new(mem, Buffer);
    buf->mem = mem;
    buf->text = text;
    array_init(&buf->lines, mem);
    compute_lines(buf);
    return buf;
}

BufferLineIter *buf_line_iter_new  (Buffer *buf, Mem *mem) {
    Auto it = mem_new(mem, BufferLineIter);
    it->buf = buf;
    if (buf->lines.count) it->text = array_get(&buf->lines, 0);
    return it;
}

Bool buf_line_iter_next (BufferLineIter *it) {
    it->idx++;
    if (it->idx == it->buf->lines.count) return false;
    it->text = array_get(&it->buf->lines, it->idx);
    return true;
}

String buf_get_line (Buffer *buf, Mem *, U64 idx) {
    return (idx < buf->lines.count) ? array_get(&buf->lines, idx) : (String){};
}

U64 buf_get_widest_line (Buffer *buf) {
    return buf->widest_line;
}
