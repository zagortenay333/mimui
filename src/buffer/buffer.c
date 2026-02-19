#include "buffer/buffer.h"
#include "os/fs.h"
#include "os/time.h"

istruct (Buf) {
    Mem *mem;
    ArrayChar data;
    ArrayString lines;
    U32 widest_line;
    Bool dirty;
};

static Void compute_aux (Buf *buf) {
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
    buf_insert(buf, 0, text);
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

static String get_line (Buf *buf, Char *p) {
    U64 n = (buf->data.data + buf->data.count) - p;
    Char *e = memchr(p, '\n', n);
    return (String){p, e ? cast(U64, e-p) : n};
}

BufLineIter *buf_line_iter_new (Buf *buf, Mem *mem) {
    Auto it = mem_new(mem, BufLineIter);
    it->buf = buf;
    if (buf->data.count) {
        it->idx = 0;
        it->text = get_line(buf, buf->data.data);
        it->offset = it->text.data - buf->data.data;
    } else {
        it->done = true;
    }
    return it;
}

Bool buf_line_iter_next (BufLineIter *it) {
    if (it->done) return true;
    it->idx++;
    it->text = get_line(it->buf, it->text.data + it->text.count + 1);
    it->offset = it->text.data - it->buf->data.data;
    if (it->text.data >= it->buf->data.data + it->buf->data.count) it->done = true;
    return it->done;
}

Void buf_insert (Buf *buf, U64 offset, String str) {
    array_insert_many(&buf->data, &str, offset);
    buf->dirty = true;
}

Void buf_delete (Buf *buf, U64 offset, U64 count) {
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

String buf_get_slice (Buf *buf, U64 offset, U64 count) {
    return str_slice(buf->data.as_slice, offset, count);
}

Bool buf_ends_with_newline (Buf *buf) {
    return str_ends_with(buf->data.as_slice, str("\n"));
}

U64 buf_find_prev_word (Buf *buf, U64 from) {
    Char *start = buf->data.data;
    Char *p = array_ref(&buf->data, from);

    if (p > start) p--;

    while (p > start) {
        if (is_whitespace(*p)) p--;
        else break;
    }

    if (is_word_char(*p)) {
        while (p > start) {
            if (is_word_char(*p)) p--;
            else break;
        }
        if (! is_word_char(*p)) p++;
    }

    return p - start;
}

U64 buf_find_next_word (Buf *buf, U64 from) {
    Char *end = &buf->data.data[buf->data.count - 1];
    Char *p = array_ref(&buf->data, from);

    while (p < end) {
        if (is_whitespace(*p)) p++;
        else break;
    }

    if (is_word_char(*p)) {
        while (p < end) {
            if (is_word_char(*p)) p++;
            else break;
        }
    } else if (p < end) {
        p++;
    }

    return p - buf->data.data;
}
