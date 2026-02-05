#include "buffer/buffer.h"
#include "os/fs.h"
#include "os/time.h"

static Void compute_stats (Buf *buf) {
    if (! buf->stats_outdated) return;
    buf->stats_outdated = false;

    buf->widest_line = 0;
    buf->line_count = 0;

    tmem_new(tm);
    buf_iter_lines (line, buf, tm, 0) {
        buf->line_count++;
        if (line->text.count > buf->widest_line) buf->widest_line = line->text.count;
    }
}

Buf *buf_new (Mem *mem, String text) {
    Auto buf     = mem_new(mem, Buf);
    buf->mem     = mem;
    buf->str     = astr_new_cap(mem, 1*KB);
    buf->gap_min = 1*KB;
    buf->stats_outdated = true;
    buf_insert(buf, text, 0);
    return buf;
}

Buf *buf_new_from_file (Mem *mem, String filepath) {
    U64 gap_size        = 1*KB;
    Auto buf            = mem_new(mem, Buf);
    String file         = fs_read_entire_file(mem, filepath, gap_size);
    buf->str.mem        = mem;
    buf->str.data       = file.data;
    buf->str.count      = file.count + gap_size + 1;
    buf->str.capacity   = file.count + gap_size + 1;
    buf->gap_min        = 1*KB;
    buf->gap_count      = gap_size + 1;
    buf->gap_idx        = file.count;
    buf->stats_outdated = true;
    return buf;
}

static String get_line (Char *p, String text) {
    Char *end = text.data + text.count;
    if (p >= end) return (String){ .data=end, .count=0 };
    Char *nl = memchr(p, '\n', end - p);
    return (String){ .data=p, .count=((nl?nl:end) - p) };
}

static Char *find_start_of_nth_line (String buf, U64 n) {
    if (n == 0) return buf.data;

    Char *p = buf.data;
    Char *end = buf.data + buf.count;
    U64 count = 0;

    while (p < end) {
        Char *nl = memchr(p, '\n', end - p);
        if (!nl) break;
        p = nl + 1;
        count++;
        if (count == n) return p;
    }

    return end;
}

BufLineIter *buf_line_iter_new (Buf *buf, Mem *mem, U64 from) {
    String d = buf_get_str(buf, 0);
    Char *p = find_start_of_nth_line(d, from);
    Auto it = mem_new(mem, BufLineIter);
    it->buf = buf;
    it->idx = from;
    it->text = get_line(p, d);
    return it;
}

Bool buf_line_iter_next (BufLineIter *it) {
    if (it->done) return false;
    it->idx++;
    String d = buf_get_str(it->buf, 0);
    it->text = get_line(it->text.data + it->text.count + 1, d);
    it->done = it->text.data == d.data + d.count;
    return it->done;
}

String buf_get_line (Buf *buf, Mem *mem, U64 idx) {
    buf_iter_lines (line, buf, mem, idx) return line->text;
    return (String){};
}

U64 buf_get_widest_line (Buf *buf) {
    compute_stats(buf);
    return buf->widest_line;
}

U64 buf_get_line_count (Buf *buf) {
    compute_stats(buf);
    return buf->line_count;
}

static Void print_state (Buf *buf) {
    String before_gap = { buf->str.data, buf->gap_idx };
    U64 n = buf->gap_idx + buf->gap_count;
    String after_gap  = { buf->str.data + n, buf->str.count - n };
    printf(
        "%.*s" TERM_CYAN("[ i=%lu n=%lu ]") "%.*s" TERM_RED("[ n=%lu cap=%lu ]\n"),
        STR(before_gap),
        buf->gap_idx,
        buf->gap_count,
        STR(after_gap),
        buf->str.count,
        buf->str.capacity
    );
}

static Void move_gap (Buf *buf, U64 idx) {
    if (idx <= buf->gap_idx) {
        Auto p = buf->str.data + idx;
        memmove(p + buf->gap_count, p, buf->gap_idx - idx);
    } else {
        Auto i = idx + buf->gap_count;
        Auto p = buf->str.data + buf->gap_idx + buf->gap_count;
        memmove(p - buf->gap_count, p, i - buf->gap_idx - buf->gap_count);
    }

    buf->gap_idx = idx;
}

static Void move_gap_to_end (Buf *buf) {
    move_gap(buf, buf_get_count(buf));
}

// Call this to increase the size of the gap region to min
// 'cap' when you have a good estimate about the amount of
// text you are about to insert.
//
// Note that calling buf_delete() after this function can
// undo the effect of this function by shrinking the gap.
Void buf_set_gap_size (Buf *buf, U64 cap) {
    if (buf->gap_count >= cap) return;
    U64 inc = buf->gap_min + (cap - buf->gap_count);
    U64 to_move = buf->str.count - buf->gap_count - buf->gap_idx;
    array_ensure_count(&buf->str, buf->str.count + inc, false);
    Char *p = buf->str.data + buf->gap_idx + buf->gap_count;
    memmove(p + inc, p, to_move);
    buf->gap_count += inc;
}

// The idx parameter does not include the gap region.
// After the insert the first char of str is at idx.
Void buf_insert (Buf *buf, String str, U64 idx) {
    buf->stats_outdated = true;
    idx = min(idx, buf->str.count - buf->gap_count);
    buf_set_gap_size(buf, str.count);
    move_gap(buf, idx);
    memcpy(buf->str.data + buf->gap_idx, str.data, str.count);
    buf->gap_idx += str.count;
    buf->gap_count -= str.count;
}

// The idx parameter does not include the gap region.
Void buf_delete (Buf *buf, U64 count, U64 idx) {
    buf->stats_outdated = true;

    idx = min(idx, buf->str.count - buf->gap_count);
    count = min(count, buf->str.count - buf->gap_count - idx);
    move_gap(buf, idx + count);
    buf->gap_idx -= count;
    buf->gap_count += count;

    if (buf->gap_count > (4 * buf->gap_min)) {
        move_gap_to_end(buf);
        U64 n = buf->gap_count - buf->gap_min;
        buf->gap_count -= n;
        buf->str.count -= n;
        buf->str.capacity += n;
        array_maybe_decrease_capacity(&buf->str);
    }
}

U64 buf_get_count (Buf *buf) {
    return buf->str.count - buf->gap_count;
}

String buf_get_str (Buf *buf, Mem *) {
    move_gap_to_end(buf);
    return (String){ .data=buf->str.data, .count=buf_get_count(buf) };
}

// The line is 1-indexed and the offset is 0-indexed.
U64 buf_line_to_offset (Buf *buf, U64 line) {
    if (line == 1) return 0;
    String s = buf_get_str(buf, 0);
    U64 l = 1;
    array_iter (c, &s) if ((c == '\n') && (++l == line)) return ARRAY_IDX + 1;
    return 0;
}
