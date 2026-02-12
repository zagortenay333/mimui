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
    buf_insert(buf, &(BufCursor){}, text);
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

U32 buf_line_col_to_offset (Buf *buf, U32 line, U32 column) {
    String line_text = buf_get_line(buf, 0, line);
    if (! line_text.data) return 0;
    U32 line_off = 0;
    U32 idx = 0;
    str_utf8_iter (c, line_text) {
        if (idx == column) break;
        line_off += c.decode.inc;
        idx++;
    }
    return (line_text.data - buf->data.data) + line_off;
}

Void buf_offset_to_line_col (Buf *buf, BufCursor *cursor) {
    compute_aux(buf);

    cursor->line = buf->lines.count - 1;
    cursor->column = 0;

    array_iter (line, &buf->lines) {
        U32 end_of_line = (line.data + line.count) - buf->data.data;
        if (end_of_line >= cursor->byte_offset) {
            cursor->line = ARRAY_IDX;
            break;
        }
    }

    String line = buf_get_line(buf, 0, cursor->line);
    U32 off = line.data - buf->data.data;
    str_utf8_iter (c, line) {
        if (off >= cursor->byte_offset) break;
        off += c.decode.inc;
        cursor->column++;
    }
}

Void buf_insert (Buf *buf, BufCursor *cursor, String str) {
    if (cursor->byte_offset != cursor->selection_offset) buf_delete(buf, cursor);
    array_insert_many(&buf->data, &str, cursor->byte_offset);
    buf->dirty = true;
    cursor->byte_offset += str.count;
    cursor->selection_offset = cursor->byte_offset;
    buf_offset_to_line_col(buf, cursor);
    cursor->preferred_column = cursor->column;
}

Void buf_delete (Buf *buf, BufCursor *cursor) {
    if (cursor->byte_offset > cursor->selection_offset) buf_cursor_swap_offset(buf, cursor);
    array_remove_many(&buf->data, cursor->byte_offset, cursor->selection_offset - cursor->byte_offset);
    buf->dirty = true;
    cursor->selection_offset = cursor->byte_offset;
    cursor->preferred_column = cursor->column;
}

U32 buf_get_count (Buf *buf) {
    return buf->data.count;
}

String buf_get_str (Buf *buf, Mem *) {
    return buf->data.as_slice;
}

BufCursor buf_cursor_new (Buf *buf, U32 line, U32 column) {
    BufCursor cursor = {};
    cursor.line = line;
    cursor.column = column;
    cursor.preferred_column = column;
    cursor.byte_offset = buf_line_col_to_offset(buf, line, column);
    cursor.selection_offset = cursor.byte_offset;
    return cursor;
}

Void buf_cursor_swap_offset (Buf *buf, BufCursor *cursor) {
    swap(cursor->byte_offset, cursor->selection_offset);
    buf_offset_to_line_col(buf, cursor);
}

Void buf_cursor_move_left (Buf *buf, BufCursor *cursor, Bool move_selection) {
    if (cursor->column > 0) {
        cursor->preferred_column--;
    } else if (cursor->line > 0) {
        cursor->line--;
        String line = buf_get_line(buf, 0, cursor->line);
        cursor->preferred_column = str_codepoint_count(line);
    }

    cursor->column = cursor->preferred_column;
    cursor->byte_offset = buf_line_col_to_offset(buf, cursor->line, cursor->column);
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void buf_cursor_move_right (Buf *buf, BufCursor *cursor, Bool move_selection) {
    String line = buf_get_line(buf, 0, cursor->line);
    U32 count = str_codepoint_count(line);

    if (cursor->preferred_column < count) {
        cursor->preferred_column++;
        cursor->column = cursor->preferred_column;
    } else if (cursor->line < buf_get_line_count(buf)-1) {
        cursor->line++;
        cursor->column = 0;
        cursor->preferred_column = 0;
    }

    cursor->byte_offset = buf_line_col_to_offset(buf, cursor->line, cursor->column);
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void buf_cursor_move_up (Buf *buf, BufCursor *cursor, Bool move_selection) {
    if (cursor->line > 0) cursor->line--;

    String line = buf_get_line(buf, 0, cursor->line);
    U32 count = str_codepoint_count(line);
    if (cursor->preferred_column > count) {
        cursor->column = count;
    } else {
        cursor->column = cursor->preferred_column;
    }

    cursor->byte_offset = buf_line_col_to_offset(buf, cursor->line, cursor->column);
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void buf_cursor_move_left_word (Buf *buf, BufCursor *cursor, Bool move_selection) {
    Char *start = buf->data.data;
    Char *p = array_ref(&buf->data, cursor->byte_offset);

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

    cursor->byte_offset = p - start;
    buf_offset_to_line_col(buf, cursor);
    cursor->preferred_column = cursor->column;
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void buf_cursor_move_right_word (Buf *buf, BufCursor *cursor, Bool move_selection) {
    Char *end = &buf->data.data[buf->data.count - 1];
    Char *p = array_ref(&buf->data, cursor->byte_offset);

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

    cursor->byte_offset = p - buf->data.data;
    buf_offset_to_line_col(buf, cursor);
    cursor->preferred_column = cursor->column;
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void buf_cursor_move_down (Buf *buf, BufCursor *cursor, Bool move_selection) {
    if (cursor->line < buf_get_line_count(buf)-1) cursor->line++;

    String line = buf_get_line(buf, 0, cursor->line);
    U32 count = str_codepoint_count(line);
    if (cursor->preferred_column > count) {
        cursor->column = count;
    } else {
        cursor->column = cursor->preferred_column;
    }

    cursor->byte_offset = buf_line_col_to_offset(buf, cursor->line, cursor->column);
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void buf_cursor_move_to_start (Buf *buf, BufCursor *cursor, Bool move_selection) {
    cursor->byte_offset = 0;
    cursor->line = 0;
    cursor->column = 0;
    cursor->preferred_column = 0;
    if (move_selection) cursor->selection_offset = 0;
}

Void buf_cursor_move_to_end (Buf *buf, BufCursor *cursor, Bool move_selection) {
    cursor->byte_offset = buf->data.count;
    buf_offset_to_line_col(buf, cursor);
    cursor->preferred_column = cursor->column;
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Bool buf_cursor_at_end_no_newline (Buf *buf, BufCursor *cursor) {
    return (cursor->byte_offset == buf->data.count) && !str_ends_with(buf->data.as_slice, str("\n")) ;
}
