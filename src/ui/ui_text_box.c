#include "ui/ui_text_box.h"
#include "ui/ui_widgets.h"
#include "base/string.h"

istruct (VisualLine) {
    U64 logical_line_offset; // Byte offset of containing logical line.
    U64 logical_line_count; // Byte length of logical line.
    U64 logical_col; // Column in logical line counting codepoints.
    U64 offset; // Byte offset of visual line.
    U64 count; // Byte length of visual line.
};

istruct (Cursor) {
    U32 byte_offset;
    U32 selection_offset;
    U32 line; // 0-indexed
    U32 column; // 0-indexed and counting codepoints not bytes.
    U32 preferred_column;
};

istruct (TextBox) {
    Mem *mem;
    Buf *buf;
    Cursor cursor;
    Vec2 cursor_coord;
    Vec2 scroll_coord;
    Vec2 scroll_coord_n;
    F32 total_width;
    F32 total_height;
    Bool dragging;
    Bool single_line_mode;
    Bool dirty;
    UiTextBoxWrapMode wrap_mode;
    Array(VisualLine) visual_lines;
    U64 widest_line;
    U64 viewport_width;
    U64 char_width;
};

static Vec2 text_box_cursor_to_coord (TextBox *info, UiBox *box, Cursor *pos);
static Cursor text_box_coord_to_cursor (TextBox *info, UiBox *box, Vec2 coord);

static Void compute_visual_lines (TextBox *info) {
    if (! info->dirty) return;
    info->dirty = false;

    info->visual_lines.count = 0;
    info->widest_line = 0;

    U64 viewport_width_in_chars = info->viewport_width / info->char_width;
    if (viewport_width_in_chars == 0) viewport_width_in_chars = 1;

    tmem_new(tm);
    buf_iter_lines (line, info->buf, tm, 0) {
        switch (info->wrap_mode) {
        case LINE_WRAP_NONE: {
            U64 logical_len = str_codepoint_count(line->text);
            if (logical_len > info->widest_line) info->widest_line = logical_len;
            badpath;
        } break;

        case LINE_WRAP_WORD: {
            badpath;
        } break;

        case LINE_WRAP_CHAR: {
            U64 col = 0;
            U64 total_col = 0;
            U64 vcount = 0;
            U64 voffset = line->offset;

            str_utf8_iter (c, line->text) {
                if (col == viewport_width_in_chars) {
                    VisualLine *vline = array_push_slot(&info->visual_lines);
                    vline->logical_line_offset = line->offset;
                    vline->logical_line_count = line->text.count;
                    vline->logical_col = total_col;
                    vline->offset = voffset;
                    vline->count = vcount;

                    voffset += vcount;
                    vcount = 0;
                    col = 0;
                }

                col++;
                total_col++;
                vcount += c.decode.inc;
            }

            if (vcount || line->text.count == 0) {
                VisualLine *vline = array_push_slot(&info->visual_lines);
                vline->logical_line_offset = line->offset;
                vline->logical_line_count = line->text.count;
                vline->logical_col = total_col;
                vline->offset = voffset;
                vline->count = vcount;
            }
        } break;
        }
    }
}

static String get_line_text (TextBox *info, U64 idx) {
    VisualLine *line = array_ref(&info->visual_lines, idx);
    return buf_get_range(info->buf, line->offset, line->count);
}

U32 cursor_line_col_to_offset (TextBox *info, Cursor *cursor) {
    VisualLine *line = array_ref(&info->visual_lines, cursor->line);
    String line_text = buf_get_range(info->buf, line->offset, line->count);
    if (line_text.count == 0) return 0;
    U32 off = 0;
    U32 idx = 0;
    str_utf8_iter (c, line_text) {
        if (idx == cursor->column) break;
        off += c.decode.inc;
        idx++;
    }
    return line->offset + off;
}

Void cursor_offset_to_line_col (TextBox *info, Cursor *cursor) {
    compute_visual_lines(info);

    cursor->line = info->visual_lines.count - 1;
    cursor->column = 0;

    array_iter (line, &info->visual_lines, *) {
        U32 end_of_line = line->offset + line->count;
        if (end_of_line >= cursor->byte_offset) {
            cursor->line = ARRAY_IDX;
            break;
        }
    }

    VisualLine *line = array_ref(&info->visual_lines, cursor->line);
    String line_text = buf_get_range(info->buf, line->offset, line->count);
    U64 off = line->offset;
    str_utf8_iter (c, line_text) {
        if (off >= cursor->byte_offset) break;
        off += c.decode.inc;
        cursor->column++;
    }
}

Void cursor_swap_offset (TextBox *info, Cursor *cursor) {
    swap(cursor->byte_offset, cursor->selection_offset);
    cursor_offset_to_line_col(info, cursor);
}

Void cursor_delete (TextBox *info, Cursor *cursor) {
    if (cursor->byte_offset > cursor->selection_offset) cursor_swap_offset(info, cursor);
    buf_delete(info->buf, cursor->byte_offset, cursor->selection_offset - cursor->byte_offset);
    info->dirty = true;
    cursor->selection_offset = cursor->byte_offset;
    cursor->preferred_column = cursor->column;
}

Void cursor_insert (TextBox *info, Cursor *cursor, String str) {
    if (cursor->byte_offset != cursor->selection_offset) cursor_delete(info, cursor);
    buf_insert(info->buf, cursor->byte_offset, str);
    info->dirty = true;
    cursor->byte_offset += str.count;
    cursor->selection_offset = cursor->byte_offset;
    cursor_offset_to_line_col(info, cursor);
    cursor->preferred_column = cursor->column;
}

Cursor cursor_new (TextBox *info, U32 line, U32 column) {
    Cursor cursor = {};
    cursor.line = line;
    cursor.column = column;
    cursor.preferred_column = column;
    cursor.byte_offset = cursor_line_col_to_offset(info, &cursor);
    cursor.selection_offset = cursor.byte_offset;
    return cursor;
}

Void cursor_move_left (TextBox *info, Cursor *cursor, Bool move_selection) {
    if (cursor->column > 0) {
        cursor->preferred_column--;
    } else if (cursor->line > 0) {
        cursor->line--;
        String line = get_line_text(info, cursor->line);
        cursor->preferred_column = str_codepoint_count(line);
    }

    cursor->column = cursor->preferred_column;
    cursor->byte_offset = cursor_line_col_to_offset(info, cursor);
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void cursor_move_right (TextBox *info, Cursor *cursor, Bool move_selection) {
    String line = get_line_text(info, cursor->line);
    U32 count = str_codepoint_count(line);

    if (cursor->preferred_column < count) {
        cursor->preferred_column++;
        cursor->column = cursor->preferred_column;
    } else if (cursor->line < info->visual_lines.count - 1) {
        cursor->line++;
        cursor->column = 0;
        cursor->preferred_column = 0;
    }

    cursor->byte_offset = cursor_line_col_to_offset(info, cursor);
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void cursor_move_up (TextBox *info, Cursor *cursor, Bool move_selection) {
    if (cursor->line > 0) cursor->line--;

    String line = get_line_text(info, cursor->line);
    U32 count = str_codepoint_count(line);
    if (cursor->preferred_column > count) {
        cursor->column = count;
    } else {
        cursor->column = cursor->preferred_column;
    }

    cursor->byte_offset = cursor_line_col_to_offset(info, cursor);
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void cursor_move_left_word (TextBox *info, Cursor *cursor, Bool move_selection) {
    cursor->byte_offset = buf_find_prev_word(info->buf, cursor->byte_offset);
    cursor_offset_to_line_col(info, cursor);
    cursor->preferred_column = cursor->column;
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void cursor_move_right_word (TextBox *info, Cursor *cursor, Bool move_selection) {
    cursor->byte_offset = buf_find_next_word(info->buf, cursor->byte_offset);
    cursor_offset_to_line_col(info, cursor);
    cursor->preferred_column = cursor->column;
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void cursor_move_down (TextBox *info, Cursor *cursor, Bool move_selection) {
    if (cursor->line < info->visual_lines.count - 1) cursor->line++;

    String line = get_line_text(info, cursor->line);
    U32 count = str_codepoint_count(line);
    if (cursor->preferred_column > count) {
        cursor->column = count;
    } else {
        cursor->column = cursor->preferred_column;
    }

    cursor->byte_offset = cursor_line_col_to_offset(info, cursor);
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void cursor_move_to_start (TextBox *info, Cursor *cursor, Bool move_selection) {
    cursor->byte_offset = 0;
    cursor->line = 0;
    cursor->column = 0;
    cursor->preferred_column = 0;
    if (move_selection) cursor->selection_offset = 0;
}

Void cursor_move_to_end (TextBox *info, Cursor *cursor, Bool move_selection) {
    cursor->byte_offset = buf_get_count(info->buf);
    cursor_offset_to_line_col(info, cursor);
    cursor->preferred_column = cursor->column;
    if (move_selection) cursor->selection_offset = cursor->byte_offset;
}

Void cursor_clamp (TextBox *info, Cursor *cursor) {
    if (cursor->byte_offset > buf_get_count(info->buf)) {
        cursor_move_to_end(info, cursor, true);
    }
}

String cursor_get_selection (TextBox *info, Cursor *cursor) {
    U32 start = cursor->byte_offset;
    U32 end = cursor->selection_offset;
    if (start > end) swap(start, end);
    return buf_get_range(info->buf, start, end - start);
}


static Void draw_line (TextBox *info, UiBox *box, U64 line_idx, VisualLine *line, Vec4 color, F32 x, F32 y) {
    tmem_new(tm);
    dr_bind_texture(&ui->font->atlas_texture);

    String line_text = buf_get_range(info->buf, line->offset, line->count);

    U32 cell_w = ui->font->width;
    U32 cell_h = ui->font->height;
    SliceGlyphInfo infos = font_get_glyph_infos(ui->font, tm, line_text);

    x = floor(x - info->scroll_coord.x);

    F32 descent = cast(F32, ui->font->descent);
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

    U64 selection_start = info->cursor.byte_offset;
    U64 selection_end   =  info->cursor.selection_offset;
    if (selection_end < selection_start) swap(selection_start, selection_end);

    U32 col_idx = 0;
    array_iter (glyph_info, &infos, *) {
        if (x > box->rect.x + box->rect.w) break;

        if (x + cell_w > box->rect.x) {
            Cursor current = cursor_new(info, line_idx, col_idx);
            Bool selected = current.byte_offset >= selection_start && current.byte_offset < selection_end;

            if (selected) dr_rect(
                .color        = ui_config_get_vec4(UI_CONFIG_BG_SELECTION),
                .color2       = ui_config_get_vec4(UI_CONFIG_BG_SELECTION),
                .top_left     = {x, y - cell_h - line_spacing},
                .bottom_right = {x + cell_w, y},
            );

            AtlasSlot *slot = font_get_atlas_slot(ui->font, glyph_info);
            Vec2 top_left = {x + slot->bearing_x, y - descent - line_spacing/2 - slot->bearing_y};
            Vec2 bottom_right = {top_left.x + slot->width, top_left.y + slot->height};
            Vec4 final_text_color = selected ? ui_config_get_vec4(UI_CONFIG_TEXT_SELECTION) : color;

            dr_rect(
                .top_left     = top_left,
                .bottom_right = bottom_right,
                .texture_rect = {slot->x, slot->y, slot->width, slot->height},
                .text_color   = final_text_color,
                .text_is_grayscale = (slot->pixel_mode == FT_PIXEL_MODE_GRAY),
            );
        }

        x += cell_w;
        col_idx++;
    }
}

static Void draw (UiBox *box) {
    UiBox *container = box->parent;
    TextBox *info = ui_get_box_data(container, 0, 0);

    compute_visual_lines(info);

    if (! ui_set_font(container)) return;

    U32 cell_h = ui->font->height;
    U32 cell_w = ui->font->width;

    F32 line_spacing   = ui_config_get_f32(UI_CONFIG_LINE_SPACING);
    info->total_width  = info->widest_line * cell_w;
    info->total_height = info->visual_lines.count * (cell_h + line_spacing);

    F32 line_height = cell_h + line_spacing;
    Cursor pos = text_box_coord_to_cursor(info, box, box->rect.top_left);
    F32 y = box->rect.y + line_height;

    array_iter_from (line, &info->visual_lines, pos.line, *) {
        if (y - line_height > box->rect.y + box->rect.h) break;
        draw_line(info, box, ARRAY_IDX, line, container->style.text_color, box->rect.x, floor(y));
        y += line_height;
    }

    if (box->signals.focused) dr_rect(
        .color = ui_config_get_vec4(UI_CONFIG_MAGENTA_1),
        .color2 = {-1},
        .top_left = info->cursor_coord,
        .bottom_right = { info->cursor_coord.x + 2, info->cursor_coord.y + cell_h },
    );
}

static Void text_box_vscroll (TextBox *info, UiBox *box, U32 line, UiAlign align) {
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);
    U32 cell_h = ui->font->height;
    info->scroll_coord_n.y = cast(F32, line) * (cell_h + line_spacing);

    F32 visible_h = box->rect.h;

    if (info->total_height <= visible_h) {
        info->scroll_coord_n.y = 0;
    } else if (align == UI_ALIGN_MIDDLE) {
        info->scroll_coord_n.y -= round(visible_h / 2);
    } else if (align == UI_ALIGN_END) {
        info->scroll_coord_n.y -= visible_h - cell_h - line_spacing;
    }
}

static Void text_box_hscroll (TextBox *info, UiBox *box, U32 column, UiAlign align) {
    U32 cell_w = ui->font->width;
    info->scroll_coord_n.x = cast(F32, column) * cell_w;

    F32 visible_w = box->rect.w;

    if (info->total_width <= visible_w) {
        info->scroll_coord_n.x = 0;
    } else if (align == UI_ALIGN_MIDDLE) {
        info->scroll_coord_n.x -= round(visible_w / 2);
    } else if (align == UI_ALIGN_END) {
        info->scroll_coord_n.x -= visible_w - cell_w;
    }
}

static Void text_box_scroll_into_view (TextBox *info, UiBox *box, Cursor *pos, U32 padding) {
    U32 cell_w = ui->font->width;
    U32 cell_h = ui->font->height;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

    Vec2 coord = text_box_cursor_to_coord(info, box, pos);

    U32 x_padding = padding * cell_w;
    U32 y_padding = padding * (cell_h + line_spacing);

    if (coord.x < box->rect.x + x_padding) {
        text_box_hscroll(info, box, sat_sub32(pos->column, padding), UI_ALIGN_START);
    } else if (coord.x > box->rect.x + box->rect.w - x_padding) {
        text_box_hscroll(info, box, clamp(sat_add32(pos->column, padding), 0u, info->widest_line), UI_ALIGN_END);
    }

    if (coord.y < box->rect.y + y_padding) {
        text_box_vscroll(info, box, sat_sub32(pos->line, padding), UI_ALIGN_START);
    } else if (coord.y + cell_h > box->rect.y + box->rect.h - y_padding) {
        text_box_vscroll(info, box, clamp(sat_add32(pos->line, padding), 0u, info->visual_lines.count - 1), UI_ALIGN_END);
    }
}

static Cursor text_box_coord_to_cursor (TextBox *info, UiBox *box, Vec2 coord) {
    U32 column = 0;

    F32 cell_w = ui->font->width;
    F32 cell_h = ui->font->height;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

    coord.x = coord.x - box->rect.x + info->scroll_coord.x;
    coord.y = coord.y - box->rect.y + info->scroll_coord.y;

    U32 line_idx = clamp(coord.y / (cell_h + line_spacing), cast(F32, 0), cast(F32, info->visual_lines.count - 1));

    VisualLine *line = array_ref(&info->visual_lines, line_idx);
    String line_text = buf_get_range(info->buf, line->offset, line->count);

    U32 max_col = str_codepoint_count(line_text);
    column = clamp(round(coord.x / cell_w), 0u, max_col);

    return cursor_new(info, line_idx, column);
}

static Vec2 text_box_cursor_to_coord (TextBox *info, UiBox *box, Cursor *pos) {
    Vec2 coord = {};

    F32 char_width  = ui->font->width;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);
    F32 line_height = ui->font->height + line_spacing;

    coord.y = pos->line * line_height + line_spacing/2;

    VisualLine *line = array_ref(&info->visual_lines, pos->line);
    String line_text = buf_get_range(info->buf, line->offset, line->count);

    U32 i = 0;
    str_utf8_iter (it, line_text) {
        if (i >= pos->column) break;
        coord.x += char_width;
        i++;
    }

    coord.x += box->rect.x - info->scroll_coord.x;
    coord.y += box->rect.y - info->scroll_coord.y;

    return coord;
}

UiBox *ui_text_box (String label, Buf *buf, Bool single_line_mode) {
    UiBox *container = ui_box_str(0, label) {
        TextBox *info = ui_get_box_data(container, sizeof(TextBox), sizeof(TextBox));
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);

        info->buf = buf;
        info->single_line_mode = single_line_mode;
        info->char_width = font->width;

        if (container->start_frame == ui->frame) {
            info->wrap_mode = LINE_WRAP_CHAR;
            info->dirty = true;
            array_init(&info->visual_lines, info->mem);
        }

        cursor_clamp(info, &info->cursor); // In case the buffer changed.

        ui_set_font(container);
        ui_style_font(UI_FONT, font);
        ui_style_f32(UI_FONT_SIZE, font->size);

        F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

        if (info->single_line_mode) {
            U32 height = 2*container->style.padding.y + (ui->font ? ui->font->height : 12) + line_spacing;
            ui_style_box_size(container, UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, height, 1});
        }

        F32 visible_w = container->rect.w - 2*container->style.padding.x;
        F32 visible_h = container->rect.h - 2*container->style.padding.y;
        Bool scroll_y = info->total_height > visible_h && visible_h > 0;
        Bool scroll_x = info->total_width  > visible_w && visible_w > 0;
        F32 scrollbar_width = ui_config_get_f32(UI_CONFIG_SCROLLBAR_WIDTH);

        UiBox *text_box = ui_box(UI_BOX_CAN_FOCUS|UI_BOX_REACTIVE|UI_BOX_CLIPPING, "text") {
            if (info->viewport_width != text_box->rect.w) info->dirty = true;
            info->viewport_width = text_box->rect.w;

            ui_style_u32(UI_ANIMATION, UI_MASK_HEIGHT|UI_MASK_WIDTH);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, container->rect.w - container->style.padding.x - (scroll_y ? scrollbar_width : 0), 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, container->rect.h - container->style.padding.y - (scroll_x ? scrollbar_width : 0), 1});

            if (text_box->signals.hovered && ui->event->tag == EVENT_SCROLL) {
                U32 cell_w = ui->font->width;
                U32 cell_h = ui->font->height;

                if (scroll_y && !ui_is_key_pressed(KEY_SHIFT)) {
                    info->scroll_coord_n.y -= (cell_h + line_spacing) * ui->event->y;
                    info->scroll_coord_n.y  = clamp(info->scroll_coord_n.y, 0, info->total_height - visible_h);
                    ui_eat_event();
                } else if (scroll_x) {
                    info->scroll_coord_n.x -= cell_w * ui->event->y;
                    info->scroll_coord_n.x  = clamp(info->scroll_coord_n.x, 0, info->total_width - visible_w);
                    ui_eat_event();
                }
            }

            text_box->draw_fn = draw;
        }

        if (scroll_y) {
            F32 ratio = visible_h / info->total_height;
            Rect rect = { container->rect.w - scrollbar_width, 0, scrollbar_width, container->rect.h };
            if (scroll_x) rect.h -= scrollbar_width;

            F32 max_y_offset = max(0.0f, info->total_height - visible_h);
            F32 knob_height  = rect.h * (visible_h / info->total_height);
            F32 max_knob_v   = rect.h - knob_height;
            F32 before       = (info->scroll_coord.y / max_y_offset) * max_knob_v;
            F32 after        = before;

            ui_vscroll_bar(str("scroll_bar_y"), rect, ratio, &after);
            if (before != after) info->scroll_coord.y = info->scroll_coord_n.y = clamp(after / max_knob_v, 0, 1) * max_y_offset;
        }

        if (scroll_x && !info->single_line_mode) {
            F32 ratio = visible_w / info->total_width;
            Rect rect = { 0, container->rect.h - scrollbar_width, container->rect.w, scrollbar_width };
            if (scroll_y) rect.w -= scrollbar_width;

            F32 max_x_offset = max(0.0f, info->total_width - visible_w);
            F32 knob_width   = rect.w * (visible_w / info->total_width);
            F32 max_knob_h   = rect.w - knob_width;
            F32 before       = (info->scroll_coord.x / max_x_offset) * max_knob_h;
            F32 after        = before;

            ui_hscroll_bar(str("scroll_bar_x"), rect, ratio, &after);
            if (before != after) info->scroll_coord.x = info->scroll_coord_n.x = clamp(after / max_knob_h, 0, 1) * max_x_offset;
        }

        if (text_box->signals.focused && ui->event->tag == EVENT_KEY_PRESS) {
            switch (ui->event->key) {
            case KEY_DEL:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    cursor_move_right_word(info, &info->cursor, false);
                    cursor_delete(info, &info->cursor);
                } else {
                    cursor_move_right(info, &info->cursor, false);
                    cursor_delete(info, &info->cursor);
                }
                ui_eat_event();
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                break;
            case KEY_W:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    cursor_move_left_word(info, &info->cursor, false);
                    cursor_delete(info, &info->cursor);
                    text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                    ui_eat_event();
                }
                break;
            case KEY_A:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    cursor_move_to_end(info, &info->cursor, true);
                    cursor_move_to_start(info, &info->cursor, false);
                    text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                    ui_eat_event();
                }
                break;
            case KEY_V:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    String text = win_get_clipboard_text(ui->frame_mem);
                    cursor_insert(info, &info->cursor, text);
                }
                break;
            case KEY_X:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    String text = cursor_get_selection(info, &info->cursor);
                    if (text.count) {
                        win_set_clipboard_text(text);
                        cursor_delete(info, &info->cursor);
                    }
                }
                break;
            case KEY_C:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    String text = cursor_get_selection(info, &info->cursor);
                    if (text.count) win_set_clipboard_text(text);
                }
                break;
            case KEY_RETURN:
                if (info->single_line_mode) break;

                Bool special_case = (info->cursor.byte_offset == buf_get_count(info->buf)) && !buf_ends_with_newline(info->buf);
                cursor_insert(info, &info->cursor, str("\n"));

                if (special_case) {
                    info->cursor.byte_offset--;
                    info->cursor.selection_offset--;
                    cursor_insert(info, &info->cursor, str("\n"));
                }

                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_BACKSPACE:
                if (info->cursor.byte_offset == info->cursor.selection_offset) cursor_move_left(info, &info->cursor, false);
                cursor_delete(info, &info->cursor);
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_LEFT:
                cursor_move_left(info, &info->cursor, !(ui->event->mods & KEY_MOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_RIGHT:
                cursor_move_right(info, &info->cursor, !(ui->event->mods & KEY_MOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_UP:
                cursor_move_up(info, &info->cursor, !(ui->event->mods & KEY_MOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_DOWN:
                cursor_move_down(info, &info->cursor, !(ui->event->mods & KEY_MOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            default:
                break;
            }
        }

        if (text_box->signals.clicked && ui->event->key == KEY_MOUSE_LEFT) {
            info->dragging = false;
        }

        if (text_box->signals.pressed) {
            ui_grab_focus(text_box);
            U32 soff = info->cursor.selection_offset;
            info->cursor = text_box_coord_to_cursor(info, text_box, ui->mouse);
            info->cursor.selection_offset = soff;

            if (info->dragging) {
                text_box_scroll_into_view(info, text_box, &info->cursor, 0);
            } else {
                info->dragging = true;
                info->cursor.selection_offset = info->cursor.byte_offset;
            }
        }

        if (text_box->signals.focused && ui->event->tag == EVENT_TEXT_INPUT) {
            cursor_insert(info, &info->cursor, ui->event->text);
            text_box_scroll_into_view(info, text_box, &info->cursor, 4);
            ui_eat_event();
        }

        ui_animate_vec2(&info->scroll_coord, info->scroll_coord_n, ui_config_get_f32(UI_CONFIG_ANIMATION_TIME_1));
        if (ui->font) info->cursor_coord = text_box_cursor_to_coord(info, text_box, &info->cursor);
    }

    return container;
}
