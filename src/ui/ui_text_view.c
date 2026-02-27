#include "ui/ui_text_view.h"

istruct (UiTextBox) {
    Mem *mem;
    String text;
    SliceGlyphInfo glyphs;
};

static Void draw (UiBox *box) {
    if (! ui_set_font(box)) return;

    dr_bind_texture(&ui->font->atlas_texture);

    UiTextBox *info = ui_get_box_data(box, 0, 0);
    F32 x           = box->rect.x;
    F32 y           = box->rect.y + ui->font->height;
    F32 descent     = cast(F32, ui->font->descent);

    array_iter (glyph, &info->glyphs, *) {
        AtlasSlot *slot = font_get_atlas_slot(ui->font, glyph);

        Vec2 top_left = { (x + glyph->x + slot->bearing_x), y + glyph->y - descent - slot->bearing_y };
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
        UiTextBox *info = ui_get_box_data(container, sizeof(UiTextBox), 1*KB);

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
