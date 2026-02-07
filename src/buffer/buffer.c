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

    buf->lines.count = 0;
    U64 prev_pos = 0;
    array_iter (c, &buf->data) {
        if (c != '\n') continue;
        String s = str_slice(buf->data.as_slice, prev_pos, ARRAY_IDX - prev_pos);
        if (s.count > buf->widest_line) buf->widest_line = s.count;
        array_push(&buf->lines, s);
        prev_pos = ARRAY_IDX + 1;
    }
}

Buf *buf_new (Mem *mem, String text) {
    Auto buf   = mem_new(mem, Buf);
    buf->mem   = mem;
    buf->dirty = true;
    buf_insert(buf, text, &(BufCursor){});
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
    idx = clamp(idx, 0u, buf->lines.count - 1);
    return buf->lines.count ? array_get(&buf->lines, idx) : (String){};
}

U64 buf_get_widest_line (Buf *buf) {
    compute_aux(buf);
    return buf->widest_line;
}

U64 buf_get_line_count (Buf *buf) {
    compute_aux(buf);
    return buf->lines.count;
}

U64 buf_line_col_to_offset (Buf *buf, U64 line, U64 column) {
    String line_text = buf_get_line(buf, 0, line);
    U64 line_off = 0;
    U64 idx = 0;
    str_utf8_iter (c, line_text) {
        if (idx == column) break;
        line_off += c.decode.inc;
        idx++;
    }
    return (line_text.data - buf->data.data) + line_off;
}

Void buf_offset_to_line_col (Buf *buf, BufCursor *cursor) {
    compute_aux(cursor->buf);
    cursor->line = 0;
    cursor->column = 0;
    array_iter (line, &buf->lines) { // @todo Replace this with binary search.
        U64 end_of_line = (line.data + line.count) - buf->data.data;
        if (end_of_line >= cursor->byte_offset) {
            cursor->line = ARRAY_IDX;
            U64 off = line.data - buf->data.data;
            str_utf8_iter (c, line) {
                if (off >= cursor->byte_offset) break;
                off += c.decode.inc;
                cursor->column++;
            }
            break;
        }
    }
}

Void buf_insert (Buf *buf, String str, BufCursor *cursor) {
    cursor->byte_offset = clamp(cursor->byte_offset, 0u, buf->data.count);
    array_insert_many(&buf->data, &str, cursor->byte_offset);
    buf->dirty = true;
    cursor->byte_offset += str.count;
    buf_offset_to_line_col(buf, cursor);
}

Void buf_delete (Buf *buf, U64 count, BufCursor *cursor) {
    count = clamp(count, 0u, buf->data.count - cursor->byte_offset);
    array_remove_many(&buf->data, cursor->byte_offset, count);
    buf->dirty = true;
    cursor->byte_offset -= count;
    buf_offset_to_line_col(buf, cursor);
}

U64 buf_get_count (Buf *buf) {
    return buf->data.count;
}

String buf_get_str (Buf *buf, Mem *) {
    return buf->data.as_slice;
}

BufCursor buf_cursor_new (Buf *buf, U64 line, U64 column) {
    BufCursor cursor = {};
    cursor.buf = buf;
    cursor.line = line;
    cursor.column = column;
    cursor.preferred_column = column;
    cursor.byte_offset = buf_line_col_to_offset(buf, line, column);
    cursor.selection_offset = cursor.byte_offset;
    return cursor;
}

Void buf_cursor_move_left (BufCursor *cursor) {
    if (cursor->column > 0) {
        cursor->preferred_column--;
    } else if (cursor->line > 0) {
        cursor->line--;
        String line = buf_get_line(cursor->buf, 0, cursor->line);
        cursor->preferred_column = str_codepoint_count(line);
    }

    cursor->column = cursor->preferred_column;
    cursor->byte_offset = buf_line_col_to_offset(cursor->buf, cursor->line, cursor->column);
}

Void buf_cursor_move_right (BufCursor *cursor) {
    String line = buf_get_line(cursor->buf, 0, cursor->line);
    U64 count = str_codepoint_count(line);

    if (cursor->preferred_column < count) {
        cursor->preferred_column++;
        cursor->column = cursor->preferred_column;
    } else if (cursor->line < buf_get_line_count(cursor->buf)-1) {
        cursor->line++;
        cursor->column = 0;
        cursor->preferred_column = 0;
    }

    cursor->byte_offset = buf_line_col_to_offset(cursor->buf, cursor->line, cursor->column);
}

Void buf_cursor_move_up (BufCursor *cursor) {
    if (cursor->line > 0) cursor->line--;

    String line = buf_get_line(cursor->buf, 0, cursor->line);
    U64 count = str_codepoint_count(line);
    if (cursor->preferred_column > count) {
        cursor->column = count;
    } else {
        cursor->column = cursor->preferred_column;
    }

    cursor->byte_offset = buf_line_col_to_offset(cursor->buf, cursor->line, cursor->column);
}

Void buf_cursor_move_down (BufCursor *cursor) {
    if (cursor->line < buf_get_line_count(cursor->buf)-1) cursor->line++;

    String line = buf_get_line(cursor->buf, 0, cursor->line);
    U64 count = str_codepoint_count(line);
    if (cursor->preferred_column > count) {
        cursor->column = count;
    } else {
        cursor->column = cursor->preferred_column;
    }

    cursor->byte_offset = buf_line_col_to_offset(cursor->buf, cursor->line, cursor->column);
}
