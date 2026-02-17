#include "ui/ui_text_box.h"
#include "ui/ui_widgets.h"
#include "base/string.h"

istruct (UiTextBox) {
    Mem *mem;
    Buf *buf;
    BufCursor cursor;
    Vec2 cursor_coord;
    Vec2 scroll_coord;
    Vec2 scroll_coord_n;
    F32 total_width;
    F32 total_height;
    Bool dragging;
    Bool single_line_mode;
};

static Vec2 text_box_cursor_to_coord (UiTextBox *info, UiBox *box, BufCursor *pos);
static BufCursor text_box_coord_to_cursor (UiTextBox *info, UiBox *box, Vec2 coord);

static Void text_box_draw_line (UiTextBox *info, UiBox *box, U32 line_idx, String text, Vec4 color, F32 x, F32 y) {
    tmem_new(tm);
    dr_bind_texture(&ui->font->atlas_texture);

    U32 cell_w = ui->font->width;
    U32 cell_h = ui->font->height;
    SliceGlyphInfo infos = font_get_glyph_infos(ui->font, tm, text);

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
            BufCursor current = buf_cursor_new(info->buf, line_idx, col_idx);
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

static Void text_box_draw (UiBox *box) {
    tmem_new(tm);

    UiBox *container = box->parent;
    UiTextBox *info = ui_get_box_data(container, 0, 0);

    if (! ui_set_font(container)) return;

    U32 cell_h = ui->font->height;
    U32 cell_w = ui->font->width;

    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);
    info->total_width  = buf_get_widest_line(info->buf) * cell_w;
    info->total_height = buf_get_line_count(info->buf) * (cell_h + line_spacing);

    F32 line_height = cell_h + line_spacing;
    BufCursor pos = text_box_coord_to_cursor(info, box, box->rect.top_left);
    F32 y = box->rect.y + line_height - info->scroll_coord.y + (pos.line * line_height);

    buf_iter_lines (line, info->buf, tm, pos.line) {
        if (y - line_height > box->rect.y + box->rect.h) break;
        text_box_draw_line(info, box, cast(U32, line->idx), line->text, container->style.text_color, box->rect.x, floor(y));
        y += line_height;
    }

    if (box->signals.focused) dr_rect(
        .color = ui_config_get_vec4(UI_CONFIG_MAGENTA_1),
        .color2 = {-1},
        .top_left = info->cursor_coord,
        .bottom_right = { info->cursor_coord.x + 2, info->cursor_coord.y + cell_h },
    );
}

static Void text_box_vscroll (UiTextBox *info, UiBox *box, U32 line, UiAlign align) {
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

static Void text_box_hscroll (UiTextBox *info, UiBox *box, U32 column, UiAlign align) {
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

static Void text_box_scroll_into_view (UiTextBox *info, UiBox *box, BufCursor *pos, U32 padding) {
    U32 cell_w = ui->font->width;
    U32 cell_h = ui->font->height;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

    Vec2 coord = text_box_cursor_to_coord(info, box, pos);

    U32 x_padding = padding * cell_w;
    U32 y_padding = padding * (cell_h + line_spacing);

    if (coord.x < box->rect.x + x_padding) {
        text_box_hscroll(info, box, sat_sub32(pos->column, padding), UI_ALIGN_START);
    } else if (coord.x > box->rect.x + box->rect.w - x_padding) {
        text_box_hscroll(info, box, clamp(sat_add32(pos->column, padding), 0u, buf_get_widest_line(info->buf)), UI_ALIGN_END);
    }

    if (coord.y < box->rect.y + y_padding) {
        text_box_vscroll(info, box, sat_sub32(pos->line, padding), UI_ALIGN_START);
    } else if (coord.y + cell_h > box->rect.y + box->rect.h - y_padding) {
        text_box_vscroll(info, box, clamp(sat_add32(pos->line, padding), 0u, buf_get_line_count(info->buf)-1), UI_ALIGN_END);
    }
}

static BufCursor text_box_coord_to_cursor (UiTextBox *info, UiBox *box, Vec2 coord) {
    U32 line = 0;
    U32 column = 0;

    F32 cell_w = ui->font->width;
    F32 cell_h = ui->font->height;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

    coord.x = coord.x - box->rect.x + info->scroll_coord.x;
    coord.y = coord.y - box->rect.y + info->scroll_coord.y;

    line = clamp(coord.y / (cell_h + line_spacing), cast(F32, 0), cast(F32, buf_get_line_count(info->buf)-1));

    tmem_new(tm);
    String line_text = buf_get_line(info->buf, tm, line);

    U32 max_col = str_codepoint_count(line_text);
    column = clamp(round(coord.x / cell_w), 0u, max_col);

    return buf_cursor_new(info->buf, line, column);
}

static Vec2 text_box_cursor_to_coord (UiTextBox *info, UiBox *box, BufCursor *pos) {
    Vec2 coord = {};

    F32 char_width  = ui->font->width;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);
    F32 line_height = ui->font->height + line_spacing;

    coord.y = pos->line * line_height + line_spacing/2;

    tmem_new(tm);
    String line_str = buf_get_line(info->buf, tm, pos->line);

    U32 i = 0;
    str_utf8_iter (it, line_str) {
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
        UiTextBox *info = ui_get_box_data(container, sizeof(UiTextBox), sizeof(UiTextBox));

        info->buf = buf;
        info->single_line_mode = single_line_mode;

        buf_cursor_clamp(info->buf, &info->cursor); // In case the buffer changed.

        ui_set_font(container);
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);
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

            text_box->draw_fn = text_box_draw;
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
                    buf_cursor_move_right_word(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                } else {
                    buf_cursor_move_right(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                }
                ui_eat_event();
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                break;
            case KEY_W:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    buf_cursor_move_left_word(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                    text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                    ui_eat_event();
                }
                break;
            case KEY_A:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    buf_cursor_move_to_end(info->buf, &info->cursor, true);
                    buf_cursor_move_to_start(info->buf, &info->cursor, false);
                    text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                    ui_eat_event();
                }
                break;
            case KEY_V:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    String text = win_get_clipboard_text(ui->frame_mem);
                    buf_insert(info->buf, &info->cursor, text);
                }
                break;
            case KEY_X:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    String text = buf_get_selection(info->buf, &info->cursor);
                    if (text.count) {
                        win_set_clipboard_text(text);
                        buf_delete(info->buf, &info->cursor);
                    }
                }
                break;
            case KEY_C:
                if (ui->event->mods & KEY_MOD_CTRL) {
                    String text = buf_get_selection(info->buf, &info->cursor);
                    if (text.count) win_set_clipboard_text(text);
                }
                break;
            case KEY_RETURN:
                if (info->single_line_mode) break;

                Bool special_case = buf_cursor_at_end_no_newline(info->buf, &info->cursor);
                buf_insert(info->buf, &info->cursor, str("\n"));

                if (special_case) {
                    // @todo This is a stupid hack for the case when we insert at the end
                    // of the buffer but the buffer doesn't end with a newline. We have to
                    // insert 2 newlines in that case, but the cursor ends up in a weird
                    // state.
                    info->cursor.byte_offset--;
                    info->cursor.selection_offset--;
                    buf_insert(info->buf, &info->cursor, str("\n"));
                }

                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_BACKSPACE:
                if (info->cursor.byte_offset == info->cursor.selection_offset) buf_cursor_move_left(info->buf, &info->cursor, false);
                buf_delete(info->buf, &info->cursor);
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_LEFT:
                buf_cursor_move_left(info->buf, &info->cursor, !(ui->event->mods & KEY_MOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_RIGHT:
                buf_cursor_move_right(info->buf, &info->cursor, !(ui->event->mods & KEY_MOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_UP:
                buf_cursor_move_up(info->buf, &info->cursor, !(ui->event->mods & KEY_MOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case KEY_DOWN:
                buf_cursor_move_down(info->buf, &info->cursor, !(ui->event->mods & KEY_MOD_SHIFT));
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
            buf_insert(info->buf, &info->cursor, ui->event->text);
            text_box_scroll_into_view(info, text_box, &info->cursor, 4);
            ui_eat_event();
        }

        ui_animate_vec2(&info->scroll_coord, info->scroll_coord_n, ui_config_get_f32(UI_CONFIG_ANIMATION_TIME_1));
        if (ui->font) info->cursor_coord = text_box_cursor_to_coord(info, text_box, &info->cursor);
    }

    return container;
}
