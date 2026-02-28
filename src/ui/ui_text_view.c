#include "ui/ui_text_view.h"

istruct (UiTextView) {
    Mem *mem;
    String text;
    SliceGlyphInfo glyphs;
};

U64 ui_text_view_coord_to_offset (UiBox *box, Vec2 coord) {
    UiTextView *info = ui_get_box_data(box, 0, 0);

    if (! ui_set_font(box)) return ARRAY_NIL_IDX;

    F32 start_x             = box->rect.x;
    F32 start_y             = box->rect.y + ui->font->height;
    F32 descent             = cast(F32, ui->font->descent);
    F32 max_width           = box->rect.w;
    F32 line_start_x_offset = 0;
    F32 current_y_offset    = 0;

    array_iter (glyph, &info->glyphs, *) {
        if (glyph->codepoint == '\n') {
            current_y_offset += ui->font->height;
            if (! ARRAY_ITER_DONE) line_start_x_offset = array_ref(&info->glyphs, ARRAY_IDX+1)->x;
            continue;
        }

        AtlasSlot *slot = font_get_atlas_slot(ui->font, glyph);

        F32 local_x = glyph->x - line_start_x_offset;

        if (local_x + slot->advance > max_width && local_x > 0) {
            line_start_x_offset = glyph->x;
            current_y_offset += ui->font->height;
            local_x = 0;
        }

        F32 x0 = start_x + local_x;
        F32 x1 = x0 + slot->advance;
        F32 y0 = start_y + glyph->y + current_y_offset - descent - slot->bearing_y;
        F32 y1 = y0 + ui->font->height;

        if (coord.y >= y0 && coord.y <= y1 && coord.x >= x0 && coord.x <= x1) {
            return glyph->byte_offset;
        }
    }

    return ARRAY_NIL_IDX;
}

static Void draw (UiBox *box) {
    if (! ui_set_font(box)) return;

    dr_bind_texture(&ui->font->atlas_texture);

    UiTextView *info        = ui_get_box_data(box, 0, 0);
    F32 start_x             = box->rect.x;
    F32 start_y             = box->rect.y + ui->font->height;
    F32 descent             = cast(F32, ui->font->descent);
    F32 max_width           = box->rect.w;
    F32 line_start_x_offset = 0;
    F32 current_y_offset    = 0;

    array_iter (glyph, &info->glyphs, *) {
        if (glyph->codepoint == '\n') {
            current_y_offset += ui->font->height;
            if (! ARRAY_ITER_DONE) line_start_x_offset = array_ref(&info->glyphs, ARRAY_IDX+1)->x;
            continue;
        }

        AtlasSlot *slot = font_get_atlas_slot(ui->font, glyph);

        F32 local_x = glyph->x - line_start_x_offset;

        if (local_x + slot->advance > max_width && local_x > 0) {
            line_start_x_offset = glyph->x;
            current_y_offset += ui->font->height;
            local_x = 0;
        }

        Vec2 top_left = {
            start_x + local_x + slot->bearing_x,
            start_y + glyph->y + current_y_offset - descent - slot->bearing_y
        };
        Vec2 bottom_right = {top_left.x + slot->width, top_left.y + slot->height};

        dr_rect(
            .top_left          = top_left,
            .bottom_right      = bottom_right,
            .texture_rect      = {slot->x, slot->y, slot->width, slot->height},
            .text_color        = box->style.text_color,
            .text_is_grayscale = (slot->pixel_mode == FT_PIXEL_MODE_GRAY),
        );
    }
}

UiBox *ui_text_view (UiBoxFlags flags, String id, String text) {
    UiBox *container = ui_box_str(flags, id) {
        UiTextView *info = ui_get_box_data(container, sizeof(UiTextView), 1*KB);

        Font *font = ui_config_get_font(UI_CONFIG_FONT_NORMAL);

        if (! info->text.data) {
            info->text = str_copy(info->mem, text);
            info->glyphs = font_get_glyph_infos(font, info->mem, text);
        }

        ui_style_font(UI_FONT, font);
        ui_style_f32(UI_FONT_SIZE, font->size);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});

        container->draw_fn = draw;
    }

    return container;
}
