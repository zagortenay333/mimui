#include "ui/ui_text_box.h"

istruct (UiTextBox) {
    Mem *mem;
    String text;
};

static Void draw (UiBox *box) {
    if (! ui_set_font(box)) return;

    tmem_new(tm);

    dr_bind_texture(&ui->font->atlas_texture);

    Bool first_frame     = box->start_frame == ui->frame;
    String text          = str(cast(CString, box->scratch));
    F32 x                = round(box->rect.x + box->style.padding.x);
    F32 y                = round(box->rect.y + box->rect.h - box->style.padding.y);
    U32 line_width       = 0;
    F32 descent          = cast(F32, ui->font->descent);
    F32 width            = cast(F32, ui->font->width);
    F32 x_pos            = x;
    SliceGlyphInfo infos = font_get_glyph_infos(ui->font, tm, text);

    // Compute available width:
    UiBox *parent = box->parent;
    F32 available_width = -1.0;
    if (parent->style.size.width.tag == UI_SIZE_PCT_PARENT || parent->style.size.width.tag == UI_SIZE_PIXELS) {
        available_width = parent->rect.w - 2*parent->style.padding.x;
    }

    // Compute width of ellipsis:
    SliceGlyphInfo dots_infos = font_get_glyph_infos(ui->font, tm, str("..."));
    F32 dots_width = 0;
    {
        GlyphInfo *last_info = array_ref_last(&dots_infos);
        AtlasSlot *slot = font_get_atlas_slot(ui->font, last_info);
        dots_width = ui->font->is_mono ? 3*width : (last_info->x + slot->bearing_x + last_info->x_advance);
    }

    // Compute line width:
    array_iter (info, &infos, *) {
        AtlasSlot *slot = font_get_atlas_slot(ui->font, info);
        x_pos += width;
        if (ARRAY_ITER_DONE) line_width = ui->font->is_mono ? (x_pos - x) : (info->x + slot->bearing_x + info->x_advance);
    }

    // Don't draw ellipsis if we fit in available with:
    if (available_width > 0 && line_width <= available_width) {
        available_width = -1.0;
    }

    // Draw text:
    x_pos = x;
    array_iter (info, &infos, *) {
        AtlasSlot *slot = font_get_atlas_slot(ui->font, info);

        F32 char_right_edge = ui->font->is_mono ? ((ARRAY_IDX + 1) * width) : (info->x + slot->bearing_x + info->x_advance);

        // Draw ellipsis:
        if (available_width > 0 && (char_right_edge + dots_width > available_width)) {
            F32 dot_base_x = ui->font->is_mono ? x_pos : (x + info->x);

            array_iter (info, &dots_infos, *) {
                AtlasSlot *slot = font_get_atlas_slot(ui->font, info);
                Vec2 top_left = {
                    ui->font->is_mono ? (dot_base_x + slot->bearing_x) : (dot_base_x + info->x + slot->bearing_x),
                    y + info->y - descent - slot->bearing_y
                };
                Vec2 bottom_right = {top_left.x + slot->width, top_left.y + slot->height};

                dr_rect(
                    .top_left          = top_left,
                    .bottom_right      = bottom_right,
                    .texture_rect      = {slot->x, slot->y, slot->width, slot->height},
                    .text_color        = first_frame ? vec4(0,0,0,0) : box->style.text_color,
                    .text_is_grayscale = (slot->pixel_mode == FT_PIXEL_MODE_GRAY),
                );

                if (ui->font->is_mono) dot_base_x += width;
            }

            line_width = ui->font->is_mono ? (dot_base_x - x) : (info->x + dots_width);
            break;
        }

        Vec2 top_left = {
            ui->font->is_mono ? (x_pos + slot->bearing_x) : (x + info->x + slot->bearing_x),
            y + info->y - descent - slot->bearing_y
        };
        Vec2 bottom_right = {top_left.x + slot->width, top_left.y + slot->height};

        dr_rect(
            .top_left          = top_left,
            .bottom_right      = bottom_right,
            .texture_rect      = {slot->x, slot->y, slot->width, slot->height},
            .text_color        = first_frame ? vec4(0,0,0,0) : box->style.text_color,
            .text_is_grayscale = (slot->pixel_mode == FT_PIXEL_MODE_GRAY),
        );

        x_pos += width;
    }
}

UiBox *ui_text_box (UiBoxFlags flags, String id, String text) {
    UiBox *box = ui_box_str(flags, id) {
        UiTextBox *info = ui_get_box_data(box, sizeof(UiTextBox), 1*KB);

        if (! info->text.data) {
            info->text = text;
        }

        Font *font = ui_config_get_font(UI_CONFIG_FONT_NORMAL);
        ui_style_font(UI_FONT, font);
        ui_style_f32(UI_FONT_SIZE, font->size);

        box->draw_fn = draw;
    }

    return box;
}
