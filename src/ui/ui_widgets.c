#include "ui/ui_widgets.h"
#include "ui/ui_text_box.h"
#include "base/string.h"

UiBox *ui_hspacer () {
    UiBox *box = ui_box(UI_BOX_INVISIBLE, "hspacer") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
    return box;
}

UiBox *ui_vspacer () {
    UiBox *box = ui_box(UI_BOX_INVISIBLE, "vspacer") { ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
    return box;
}

UiBox *ui_button_push (String id) {
    UiBox *container = ui_box_push_str(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, id);
    ui_tag_box(container, "button");
    ui_style_box_u32(container, UI_ALIGN_Y, UI_ALIGN_MIDDLE);
    ui_style_box_u32(container, UI_ALIGN_X, UI_ALIGN_MIDDLE);
    ui_style_box_from_config(container, UI_BG_COLOR, UI_CONFIG_FG_3);
    ui_style_box_from_config(container, UI_BG_COLOR2, UI_CONFIG_FG_4);
    ui_style_box_from_config(container, UI_RADIUS, UI_CONFIG_RADIUS_1);
    ui_style_box_from_config(container, UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
    ui_style_box_from_config(container, UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
    ui_style_box_from_config(container, UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_box_vec2(container, UI_SHADOW_OFFSETS, vec2(0, -1));
    ui_style_box_size(container, UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 1});

    ui_style_rule(".button.focus") {
        ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
        ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
    }

    ui_style_rule(".button.press") {
        ui_style_f32(UI_OUTSET_SHADOW_WIDTH, 0);
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_4);
        ui_style_from_config(UI_BG_COLOR2, UI_CONFIG_FG_3);
        ui_style_from_config(UI_INSET_SHADOW_WIDTH, UI_CONFIG_IN_SHADOW_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_COLOR, UI_CONFIG_IN_SHADOW_1_COLOR);
    }

    return container;
}

Void ui_button_pop () {
    UiBox *container = array_get_last(&ui->box_stack);

    if (container->signals.hovered) {
        ui_push_clip(container, true);
        ui_box(UI_BOX_CLICK_THROUGH, "button_highlight") {
            F32 s = container->rect.h/8;
            ui_style_f32(UI_EDGE_SOFTNESS, 60);
            ui_style_vec4(UI_RADIUS, vec4(s, s, s, s));
            ui_style_f32(UI_FLOAT_X, ui->mouse.x - container->rect.x - s);
            ui_style_f32(UI_FLOAT_Y, ui->mouse.y - container->rect.y - s);
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_HIGHLIGHT);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 2*s, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 2*s, 1});
        }
        ui_pop_clip();
    }

    ui_pop_parent();
}

UiBox *ui_button_label_str (String id, String label) {
    UiBox *button = ui_button(id) {
        UiBox *label_box = ui_label(UI_BOX_CLICK_THROUGH, "button_label", label);
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);
        ui_style_box_font(label_box, UI_FONT, font);
        ui_style_box_f32(label_box, UI_FONT_SIZE, font->size);
    }

    return button;
}

UiBox *ui_button_label (CString id) {
    return ui_button_label_str(str(id), str(id));
}

UiBox *ui_button_group_push (String id) {
    UiBox *container = ui_box_push_str(0, id);
    F32 r = ui_config_get_vec4(UI_CONFIG_RADIUS_1).x;
    ui_style_rule(".button") ui_style_vec4(UI_RADIUS, vec4(0, 0, 0, 0));
    ui_style_rule(".button:first") ui_style_vec4(UI_RADIUS, vec4(0, r, 0, r));
    ui_style_rule(".button:last") ui_style_vec4(UI_RADIUS, vec4(r, 0, r, 0));
    return container;
}

Void ui_button_group_pop () {
    ui_pop_parent();
}

static Void size_label (UiBox *box, U64 axis) {
    // Sizing done in the draw_label function.
}

static Void draw_label (UiBox *box) {
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

    box->rect.w = line_width + 2*box->style.padding.x;
    box->rect.h = ui->font->height + 2*box->style.padding.y;
}

UiBox *ui_label (UiBoxFlags flags, CString id, String label) {
    UiBox *box = ui_box_str(flags, str(id)) {
        Font *font = ui_config_get_font(UI_CONFIG_FONT_NORMAL);
        ui_style_font(UI_FONT, font);
        ui_style_f32(UI_FONT_SIZE, font->size);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CUSTOM, 1, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CUSTOM, 1, 1});
        box->size_fn = size_label;
        box->draw_fn = draw_label;
        box->scratch = cast(U64, cstr(ui->frame_mem, label));
    }

    return box;
}

UiBox *ui_icon (UiBoxFlags flags, CString id, U32 size, U32 icon) {
    UiBox *label = ui_label(flags, id, str_utf32_to_utf8(ui->frame_mem, icon));
    ui_style_box_from_config(label, UI_FONT, UI_CONFIG_FONT_ICONS);
    ui_style_box_f32(label, UI_FONT_SIZE, size);
    return label;
}

UiBox *ui_icon_button (UiBoxFlags flags, CString id, U32 size, U32 icon) {
    UiBox *container = ui_box(flags|UI_BOX_CAN_FOCUS|UI_BOX_REACTIVE, id) {
        ui_tag("icon_button");
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
        ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);

        ui_icon(UI_BOX_CLICK_THROUGH, "icon", size, icon);
    }

    return container;
}

UiBox *ui_checkbox (CString id, Bool *val) {
    UiBox *bg = ui_box(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, id) {
        F32 s = 20;

        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, s, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, s, 1});
        ui_style_from_config(UI_RADIUS, UI_CONFIG_RADIUS_1);
        ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
        ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_WIDTH, UI_CONFIG_IN_SHADOW_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_COLOR, UI_CONFIG_IN_SHADOW_1_COLOR);
        ui_style_u32(UI_ANIMATION, UI_MASK_BG_COLOR);

        ui_style_rule(".focus") {
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
        }

        if (*val) {
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_MAGENTA_1);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_2_COLOR);
            ui_icon(UI_BOX_CLICK_THROUGH, "mark", 16, UI_ICON_CHECK);
        } else {
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        }

        if (bg->signals.clicked) {
            *val = !*val;
        }
    }

    return bg;
}

istruct (UiImage) {
    Texture *texture;
    Bool blur;
    Vec4 tint;
    F32 pref_width;
};

static Void draw_image (UiBox *box) {
    Auto info = cast(UiImage *, box->scratch);
    dr_flush_vertices();
    dr_bind_texture(info->texture);
    dr_rect(
        .top_left          = box->rect.top_left,
        .bottom_right      = {box->rect.x + box->rect.w, box->rect.y + box->rect.h},
        .radius            = box->style.radius,
        .texture_rect      = {0, 0, info->texture->width, info->texture->height},
        .text_color        = (info->tint.w > 0) ? info->tint : vec4(1, 1, 1, 1),
        .text_is_grayscale = (info->tint.w > 0) ? 1 : 0,
    );
}

UiBox *ui_image (CString id, Texture *texture, Bool blur, Vec4 tint, F32 pref_width) {
    UiBox *img = ui_box(UI_BOX_INVISIBLE, id) {
        img->draw_fn = draw_image;
        UiImage *info = mem_new(ui->frame_mem, UiImage);
        info->texture = texture;
        info->blur = blur;
        info->tint = tint;
        info->pref_width = pref_width;
        img->scratch = cast(U64, info);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, info->pref_width, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, round(texture->height * (info->pref_width / texture->width)), 1});
        ui_style_from_config(UI_RADIUS, UI_CONFIG_RADIUS_2);

        ui_box(0, "overlay") {
            ui_style_size(UI_WIDTH, (UiSize){ UI_SIZE_PCT_PARENT, 1, 1});
            ui_style_size(UI_HEIGHT, (UiSize){ UI_SIZE_PCT_PARENT, 1, 1});
            ui_style_vec4(UI_RADIUS, img->style.radius);
            if (info->blur) ui_style_from_config(UI_BLUR_RADIUS, UI_CONFIG_BLUR);
        }
    }

    return img;
}

UiBox *ui_toggle (CString id, Bool *val) {
    UiBox *bg = ui_box(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, id) {
        F32 s = 24.0;

        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 2*s, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, s, 1});
        ui_style_vec4(UI_RADIUS, vec4(s/2, s/2, s/2, s/2));
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
        ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_WIDTH, UI_CONFIG_IN_SHADOW_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_COLOR, UI_CONFIG_IN_SHADOW_1_COLOR);
        ui_style_u32(UI_ANIMATION, UI_MASK_BG_COLOR);

        ui_style_rule(".focus") {
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
        }

        if (*val) {
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_MAGENTA_1);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_2_COLOR);
        } else {
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        }

        if (bg->signals.clicked) {
            *val = !*val;
        }

        ui_box(UI_BOX_CLICK_THROUGH, "toggle_knob") {
            F32 ks = 16.0;
            ui_style_f32(UI_EDGE_SOFTNESS, 1.3);
            ui_style_f32(UI_FLOAT_X, *val ? (2*s - ks - 4) : 4);
            ui_style_f32(UI_FLOAT_Y, 4);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, ks, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, ks, 1});
            ui_style_vec4(UI_RADIUS, vec4(ks/2, ks/2, ks/2, ks/2));
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1);
            ui_style_from_config(UI_BG_COLOR2, UI_CONFIG_FG_2);
            ui_style_from_config(UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
            ui_style_from_config(UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
            ui_style_u32(UI_ANIMATION, UI_MASK_FLOAT_X);
            ui_style_from_config(UI_ANIMATION_TIME, UI_CONFIG_ANIMATION_TIME_1);
        }
    }

    return bg;
}

UiBox *ui_vscroll_bar (String label, Rect rect, F32 ratio, F32 *val) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, label) {
        ui_style_f32(UI_FLOAT_X, rect.x);
        ui_style_f32(UI_FLOAT_Y, rect.y);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, rect.h, 0});
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
        ui_style_f32(UI_EDGE_SOFTNESS, 0);

        ui_style_rule(".hover #scroll_bar_knob") { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule(".press #scroll_bar_knob") { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule("#scroll_bar_knob.hover")  { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule("#scroll_bar_knob.press")  { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }

        F32 knob_size = round(rect.h * ratio);

        if (container->signals.pressed) {
            *val = ui->mouse.y - container->rect.y - knob_size/2;
            *val = clamp(*val, 0, rect.h - knob_size);
        }

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL)) {
            *val -= (25 * ui->event->y);
            *val = clamp(*val, 0, rect.h - knob_size);
            ui_eat_event();
        }

        ui_box(UI_BOX_CLICK_THROUGH|UI_BOX_INVISIBLE, "scroll_bar_spacer") {
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, *val, 0});
        }

        UiBox *knob = ui_box(UI_BOX_REACTIVE, "scroll_bar_knob") {
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, rect.w, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, knob_size, 1});
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_2);
            ui_style_f32(UI_EDGE_SOFTNESS, 0);

            if (knob->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE) {
                *val += ui->mouse_dt.y;
                *val = clamp(*val, 0, rect.h - knob_size);
            }
        }
    }

    return container;
}

UiBox *ui_hscroll_bar (String label, Rect rect, F32 ratio, F32 *val) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, label) {
        ui_style_f32(UI_FLOAT_X, rect.x);
        ui_style_f32(UI_FLOAT_Y, rect.y);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, rect.w, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 1});
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_u32(UI_AXIS, UI_AXIS_HORIZONTAL);
        ui_style_f32(UI_EDGE_SOFTNESS, 0);

        ui_style_rule(".hover #scroll_bar_knob") { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule(".press #scroll_bar_knob") { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule("#scroll_bar_knob.hover")  { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule("#scroll_bar_knob.press")  { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }

        F32 knob_size = rect.w * ratio;

        if (container->signals.pressed) {
            *val = ui->mouse.x - container->rect.x - knob_size/2;
            *val = clamp(*val, 0, rect.w - knob_size);
        }

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL)) {
            *val -= (25 * ui->event->y);
            *val = clamp(*val, 0, rect.w - knob_size);
            ui_eat_event();
        }

        ui_box(UI_BOX_CLICK_THROUGH|UI_BOX_INVISIBLE, "scroll_bar_spacer") {
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, *val, 0});
        }

        UiBox *knob = ui_box(UI_BOX_REACTIVE, "scroll_bar_knob") {
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, rect.h, 1});
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, knob_size, 1});
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_2);
            ui_style_f32(UI_EDGE_SOFTNESS, 0);

            if (knob->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE) {
                *val += ui->mouse_dt.x;
                *val = clamp(*val, 0, rect.w - knob_size);
            }
        }
    }

    return container;
}

UiBox *ui_scroll_box_push (String label) {
    UiBox *container = ui_box_push_str(UI_BOX_REACTIVE, label);
    ui_style_box_u32(container, UI_OVERFLOW_X, true);
    ui_style_box_u32(container, UI_OVERFLOW_Y, true);
    container->scratch = ui->depth_first.count-1;
    ui_push_clip(container, true);
    return container;
}

Void ui_scroll_box_pop () {
    UiBox *container = array_get_last(&ui->box_stack);

    Bool contains_focused = (ui->focus_idx >= container->scratch);
    if (contains_focused && ui->event->tag == EVENT_KEY_PRESS && ui->event->key == KEY_TAB) {
        F32 fx1 = ui->focused->rect.x + ui->focused->rect.w;
        F32 cx1 = container->rect.x + container->rect.w;
        if (fx1 > cx1) {
            container->content.x -= fx1 - cx1;
        } else if (ui->focused->rect.x < container->rect.x) {
            container->content.x += container->rect.x - ui->focused->rect.x;
        }

        F32 fy1 = ui->focused->rect.y + ui->focused->rect.h;
        F32 cy1 = container->rect.y + container->rect.h;
        if (fy1 > cy1) {
            container->content.y -= fy1 - cy1;
        } else if (ui->focused->rect.y < container->rect.y) {
            container->content.y += container->rect.y - ui->focused->rect.y;
        }
    }

    F32 speed = 25;
    F32 bar_width = 8;

    if (container->rect.w < container->content.w) {
        F32 scroll_val = (fabs(container->content.x) / container->content.w) * container->rect.w;
        F32 ratio = container->rect.w / container->content.w;
        ui_hscroll_bar(str("scroll_bar_x"), (Rect){0, container->rect.h - bar_width, container->rect.w, bar_width}, ratio, &scroll_val);
        container->content.x = -(scroll_val/container->rect.w*container->content.w);

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL) && ui_is_key_pressed(KEY_CTRL)) {
            container->content.x += speed * ui->event->y;
            ui_eat_event();
        }

        container->content.x = clamp(container->content.x, -(container->content.w - container->rect.w), 0);
    } else {
        container->content.x = 0;
    }

    if (container->rect.h < container->content.h) {
        F32 scroll_val = (fabs(container->content.y) / container->content.h) * container->rect.h;
        F32 ratio = container->rect.h / container->content.h;
        ui_vscroll_bar(str("scroll_bar_y"), (Rect){container->rect.w - bar_width, 0, bar_width, container->rect.h}, ratio, &scroll_val);
        container->content.y = -(scroll_val/container->rect.h*container->content.h);

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL) && !ui_is_key_pressed(KEY_CTRL)) {
            container->content.y += speed * ui->event->y;
            ui_eat_event();
        }

        container->content.y = clamp(container->content.y, -(container->content.h - container->rect.h), 0);
    } else {
        container->content.y = 0;
    }

    ui_pop_clip();
    ui_pop_parent();
}

istruct (UiPopup) {
    Bool *shown;
    Bool sideways;
    UiBox *anchor;
};

static Void size_popup (UiBox *popup, U64 axis) {
    F32 size = 0;
    Bool cycle = false;

    if (popup->style.axis == axis) {
        array_iter(child, &popup->children) {
            if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT) cycle = true;
            size += child->rect.size[axis];
        }
    } else {
        array_iter(child, &popup->children) {
            if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT) cycle = true;
            if (child->rect.size[axis] > size) size = child->rect.size[axis];
        }
    }

    if (cycle) {
        popup->rect.size[axis] = round(ui->root->rect.size[axis] / 2);
    } else {
        popup->rect.size[axis] = min(size + 2 * popup->style.padding.v[axis], ui->root->rect.size[axis] - 20.0);
    }
}

static Void layout_popup (UiBox *popup) {
    UiPopup *info = cast(UiPopup*, popup->scratch);
    Rect anchor = info->anchor->rect;
    Rect viewport = ui->root->rect;
    F32 popup_w = popup->rect.w;
    F32 popup_h = popup->rect.h;
    F32 margin = 6.0f;

    F32 space_left   = anchor.x - viewport.x;
    F32 space_right  = (viewport.x + viewport.w) - (anchor.x + anchor.w);
    F32 space_top    = anchor.y - viewport.y;
    F32 space_bottom = (viewport.y + viewport.h) - (anchor.y + anchor.h);

    // @todo Due to the complex layout logic of the popup which relies
    // on the size information from previous frames, we have to delay
    // drawing the popup for the first two frames in order to prevent
    // nasty flickering... We have to do this on top of also having to
    // use the deferred_layout_fns feature for popups...
    popup->flags &= ~UI_BOX_INVISIBLE;
    if (ui->frame - popup->start_frame < 2) popup->flags |= UI_BOX_INVISIBLE;

    enum { POPUP_LEFT, POPUP_RIGHT, POPUP_TOP, POPUP_BOTTOM } side;

    if (info->sideways) {
        side = space_left > space_right ? POPUP_LEFT : POPUP_RIGHT;
    } else {
        side = space_top > space_bottom ? POPUP_TOP : POPUP_BOTTOM;
    }

    F32 x = 0;
    F32 y = 0;

    switch (side) {
    case POPUP_RIGHT:
        x = anchor.x + anchor.w + margin;
        y = anchor.y + (anchor.h - popup_h) * 0.5f;
        break;
    case POPUP_LEFT:
        x = anchor.x - popup_w - margin;
        y = anchor.y + (anchor.h - popup_h) * 0.5f;
        break;
    case POPUP_BOTTOM:
        x = anchor.x + (anchor.w - popup_w) * 0.5f;
        y = anchor.y + anchor.h + margin;
        break;
    case POPUP_TOP:
        x = anchor.x + (anchor.w - popup_w) * 0.5f;
        y = anchor.y - popup_h - margin;
        break;
    }

    x = clamp(x, 0, viewport.w - popup_w);
    y = clamp(y, 0, viewport.h - popup_h);

    ui_style_box_f32(popup, UI_FLOAT_X, x);
    ui_style_box_f32(popup, UI_FLOAT_Y, y);
}

UiBox *ui_popup_push (String id, Bool *shown, Bool sideways, UiBox *anchor) {
    ui_push_parent(ui->root);
    ui_push_clip(ui->root, false);

    UiBox *overlay = ui_box_push_str(UI_BOX_REACTIVE, id);
    ui_style_box_f32(overlay, UI_FLOAT_X, 0);
    ui_style_box_f32(overlay, UI_FLOAT_Y, 0);
    ui_style_box_size(overlay, UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_box_size(overlay, UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});

    *shown = true;
    if ((ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == KEY_ESC)) *shown = false;
    if (overlay->signals.clicked && ui->event->key == KEY_MOUSE_LEFT) *shown = false;

    UiBox *popup = ui_scroll_box_push(str("popup"));
    popup->size_fn = size_popup;
    UiPopup *info = mem_new(ui->frame_mem, UiPopup);
    info->sideways = sideways;
    info->shown = shown;
    info->anchor = anchor;
    popup->scratch = cast(U64, info);
    array_push_lit(&ui->deferred_layout_fns, layout_popup, popup);
    ui_style_box_u32(popup, UI_AXIS, UI_AXIS_VERTICAL);
    ui_style_box_size(popup, UI_WIDTH, (UiSize){UI_SIZE_CUSTOM, 1, 0});
    ui_style_box_size(popup, UI_HEIGHT, (UiSize){UI_SIZE_CUSTOM, 1, 0});
    ui_style_box_from_config(popup, UI_BG_COLOR, UI_CONFIG_BG_4);
    ui_style_box_from_config(popup, UI_RADIUS, UI_CONFIG_RADIUS_2);
    ui_style_box_from_config(popup, UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_box_from_config(popup, UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
    ui_style_box_from_config(popup, UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
    ui_style_box_from_config(popup, UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
    ui_style_box_from_config(popup, UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
    ui_style_box_from_config(popup, UI_ANIMATION_TIME, UI_CONFIG_ANIMATION_TIME_3);
    ui_style_box_u32(popup, UI_ANIMATION, UI_MASK_BG_COLOR);
    ui_style_from_config(UI_BLUR_RADIUS, UI_CONFIG_BLUR);

    return popup;
}

Void ui_popup_pop () {
    ui_scroll_box_pop();
    ui_pop_parent();
    ui_pop_clip();
    ui_pop_parent();
}

static Void size_modal (UiBox *modal, U64 axis) {
    F32 size = 0;
    Bool cycle = false;

    if (modal->style.axis == axis) {
        array_iter(child, &modal->children) {
            if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT) cycle = true;
            size += child->rect.size[axis];
        }
    } else {
        array_iter(child, &modal->children) {
            if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT) cycle = true;
            if (child->rect.size[axis] > size) size = child->rect.size[axis];
        }
    }

    if (cycle) {
        modal->rect.size[axis] = ui->root->rect.size[axis] - 20.0;
    } else {
        modal->rect.size[axis] = min(size + 2 * modal->style.padding.v[axis], ui->root->rect.size[axis] - 20.0);
    }
}

static Void layout_modal (UiBox *modal) {
    ui_style_box_f32(modal, UI_FLOAT_X, ui->root->rect.w/2 - modal->rect.w/2);
    ui_style_box_f32(modal, UI_FLOAT_Y, ui->root->rect.h/2 - modal->rect.h/2);
}

UiBox *ui_modal_push (String id, Bool *shown) {
    ui_push_parent(ui->root);
    ui_push_clip(ui->root, false);

    UiBox *overlay = ui_box_push_str(UI_BOX_REACTIVE, id);
    ui_style_box_f32(overlay, UI_FLOAT_X, 0);
    ui_style_box_f32(overlay, UI_FLOAT_Y, 0);
    ui_style_box_size(overlay, UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_box_size(overlay, UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});

    *shown = true;
    if ((ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == KEY_ESC)) *shown = false;
    if (overlay->signals.clicked && ui->event->key == KEY_MOUSE_LEFT) *shown = false;

    UiBox *modal = ui_box_push(0, "modal");
    modal->size_fn = size_modal;
    array_push_lit(&ui->deferred_layout_fns, layout_modal, modal);
    ui_style_box_size(modal, UI_WIDTH, (UiSize){UI_SIZE_CUSTOM, 1, 0});
    ui_style_box_size(modal, UI_HEIGHT, (UiSize){UI_SIZE_CUSTOM, 1, 0});
    ui_style_box_from_config(modal, UI_BG_COLOR, UI_CONFIG_BG_4);
    ui_style_box_from_config(modal, UI_RADIUS, UI_CONFIG_RADIUS_2);
    ui_style_box_from_config(modal, UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_box_from_config(modal, UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
    ui_style_box_from_config(modal, UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
    ui_style_box_from_config(modal, UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
    ui_style_box_from_config(modal, UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
    ui_style_box_from_config(modal, UI_BLUR_RADIUS, UI_CONFIG_BLUR);

    return overlay;
}

Void ui_modal_pop () {
    ui_pop_parent();
    ui_pop_parent();
    ui_pop_clip();
    ui_pop_parent();
}

UiBox *ui_tooltip_push (String id) {
    ui_push_parent(ui->root);
    ui_push_clip(ui->root, false);

    UiBox *tooltip = ui_box_push_str(0, id);
    ui_style_box_f32(tooltip, UI_FLOAT_X, ui->mouse.x);
    ui_style_box_f32(tooltip, UI_FLOAT_Y, ui->mouse.y + 20);
    ui_style_box_from_config(tooltip, UI_BG_COLOR, UI_CONFIG_BG_4);
    ui_style_box_from_config(tooltip, UI_RADIUS, UI_CONFIG_RADIUS_2);
    ui_style_box_from_config(tooltip, UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_box_from_config(tooltip, UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
    ui_style_box_from_config(tooltip, UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
    ui_style_box_from_config(tooltip, UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
    ui_style_box_from_config(tooltip, UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
    ui_style_box_from_config(tooltip, UI_BLUR_RADIUS, UI_CONFIG_BLUR);
    ui_style_box_from_config(tooltip, UI_ANIMATION_TIME, UI_CONFIG_ANIMATION_TIME_3);
    ui_style_box_u32(tooltip, UI_ANIMATION, UI_MASK_BG_COLOR);

    return tooltip;
}

Void ui_tooltip_pop () {
    ui_pop_parent();
    ui_pop_clip();
    ui_pop_parent();
}

UiBox *ui_entry (String id, Buf *buf, F32 width_in_chars, String hint) {
    UiBox *container = ui_box_str(UI_BOX_INVISIBLE, id) {
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 1});

        UiBox *text_box = ui_tbox(str("text_box"), buf, true, LINE_WRAP_NONE);
        ui_style_box_from_config(text_box, UI_RADIUS, UI_CONFIG_RADIUS_1);
        ui_style_box_from_config(text_box, UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_box_from_config(text_box, UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
        ui_style_box_from_config(text_box, UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
        ui_style_box_from_config(text_box, UI_INSET_SHADOW_WIDTH, UI_CONFIG_IN_SHADOW_1_WIDTH);
        ui_style_box_from_config(text_box, UI_INSET_SHADOW_COLOR, UI_CONFIG_IN_SHADOW_1_COLOR);
        ui_style_box_from_config(text_box, UI_PADDING, UI_CONFIG_PADDING_1);
        F32 width = width_in_chars*(text_box->style.font ? text_box->style.font->width : 12) + 2*text_box->style.padding.x;
        ui_style_box_size(text_box, UI_WIDTH, (UiSize){UI_SIZE_PIXELS, width, 1});

        if (hint.count && buf_get_count(buf) == 0) {
            UiBox *h = ui_label(UI_BOX_CLICK_THROUGH, "hint", hint);
            UiBox *inner_text = array_get(&text_box->children, 0);
            ui_style_box_f32(h, UI_FLOAT_X, inner_text->rect.x - container->rect.x);
            ui_style_box_f32(h, UI_FLOAT_Y, inner_text->rect.y - container->rect.y + 1);
            ui_style_box_font(h, UI_FONT, text_box->next_style.font);
            ui_style_box_f32(h, UI_FONT_SIZE, text_box->next_style.font_size);
            ui_style_box_from_config(h, UI_TEXT_COLOR, UI_CONFIG_TEXT_COLOR_3);
        }
    }

    return container;
}

static CString key_to_str (Key key) {
    #define X(KEY, STR) if (key == KEY) return STR;
    EACH_KEY(X)
    #undef X
    return "Unkown";
}

UiBox *ui_shortcut_picker (String id, Key *key, KeyMod *mods) {
    tmem_new(tm);

    UiBox *container = ui_button(id) {
        ui_tag("shortcut_picker");

        Bool dim = false;
        Bool listening = container->scratch;
        String label;

        if (listening) {
            if (ui->event->tag == EVENT_KEY_PRESS &&
                ui->event->key != KEY_CTRL &&
                ui->event->key != KEY_SHIFT &&
                ui->event->key != KEY_ALT &&
                ui->event->key != KEY_MOUSE_LEFT &&
                ui->event->key != KEY_MOUSE_MIDDLE &&
                ui->event->key != KEY_MOUSE_RIGHT
            ) {
                container->scratch = false;
                *key = ui->event->key;
                *mods = ui->event->mods;
                ui_eat_event();
            }

            label = str("Type Shortcut...");
        } else {
            if (container->signals.clicked) {
                ui_grab_focus(container);
                container->scratch = true;
            }

            if (*key == KEY_UNKNOWN && *mods == 0) {
                dim = true;
                label = str("Set Shortcut");
            } else {
                CString c = (*mods & KEY_MOD_CTRL) ? "Ctrl + " : "";
                CString s = (*mods & KEY_MOD_SHIFT) ? "Shift + " : "";
                CString a = (*mods & KEY_MOD_ALT) ? "Alt + " : "";
                label = astr_fmt(tm, "%s%s%s%s",  c, s, a, key_to_str(*key));
            }
        }

        UiBox *label_box = ui_label(UI_BOX_CLICK_THROUGH, "label", label);
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);
        ui_style_box_font(label_box, UI_FONT, font);
        ui_style_box_f32(label_box, UI_FONT_SIZE, font->size);
        if (dim) ui_style_box_from_config(label_box, UI_TEXT_COLOR, UI_CONFIG_TEXT_COLOR_3);
    }

    return container;
}

UiBox *ui_dropdown (String id, U64 *selection, SliceString options) {
    UiBox *container = ui_button(id) {
        Bool opened = container->scratch;

        if (opened || container->signals.clicked) {
            ui_popup("popup", &opened, false, container) {
                array_iter (option, &options) {
                    tmem_new(tm);
                    String id = astr_fmt(tm, "button%lu", ARRAY_IDX);

                    UiBox *button = ui_button(id) {
                        ui_style_u32(UI_ALIGN_X, UI_ALIGN_START);
                        ui_style_vec4(UI_BG_COLOR, vec4(0, 0, 0, 0));
                        ui_style_vec4(UI_BG_COLOR2, vec4(-1, 0, 0, 0));
                        ui_style_f32(UI_OUTSET_SHADOW_WIDTH, 0);
                        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 200, 1});

                        if (button->signals.hovered) {
                            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_4);
                        }

                        if (button->signals.clicked) {
                            opened = false;
                            *selection = ARRAY_IDX;
                        }

                        UiBox *label = ui_label(UI_BOX_CLICK_THROUGH, "label", option);
                        if (ARRAY_IDX == *selection) ui_style_box_from_config(label, UI_TEXT_COLOR, UI_CONFIG_RED_TEXT);
                    }
                }
            }

            ui_tag_box(container, "press");
            container->scratch = opened;
        }

        *selection = clamp(*selection, 0u, options.count);
        String text = array_get(&options, *selection);
        UiBox *label_box = ui_label(UI_BOX_CLICK_THROUGH, "label", text);
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);
        ui_style_box_font(label_box, UI_FONT, font);
        ui_style_box_f32(label_box, UI_FONT_SIZE, font->size);
    }

    return container;
}

istruct (UiIntPicker) {
    Mem *mem;
    I64 val;
    Buf *buf;
};

UiBox *ui_int_picker (String id, I64 *val, I64 min, I64 max, U8 width_in_chars) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, id) {
        ui_tag("int_picker");

        UiIntPicker *info = ui_get_box_data(container, sizeof(UiIntPicker), sizeof(UiIntPicker));
        if (! info->buf) info->buf = buf_new(info->mem, str(""));

        if (container->start_frame == ui->frame || info->val != *val) {
            String str = astr_fmt(ui->frame_mem, "%li", *val);
            buf_clear(info->buf);
            buf_insert(info->buf, 0, str);
            info->val = *val;
        }

        UiBox *entry = ui_entry(str("entry"), info->buf, width_in_chars, str(""));

        Bool valid = true;
        {
            String text = buf_get_str(info->buf, ui->frame_mem);
            array_iter (c, &text) {
                if (c == '-' && ARRAY_IDX == 0) continue;
                if (c < '0' || c > '9') { valid = false; break; }
            }
            I64 v;
            if (valid) valid = str_to_i64(cstr(ui->frame_mem, text), &v, 10);
            if (valid && (v < min || v > max)) valid = false;
            if (valid) *val = v;
        }

        if (! valid) ui_style_box_from_config(entry, UI_TEXT_COLOR, UI_CONFIG_RED_TEXT);

        if (container->signals.hovered) {
            ui_tooltip("tooltip") ui_label(0, "label", astr_fmt(ui->frame_mem, "Integer in range [%li, %li].", min, max));

            if (valid && (ui->event->tag == EVENT_SCROLL)) {
                if (ui->event->y > 0) {
                    *val += 1;
                } else {
                    *val -= 1;
                }

                *val = clamp(*val, min, max);
                ui_eat_event();
            }
        }
    }

    return container;
}

UiBox *ui_time_picker (String id, Time *time, UiTimePickerMode mode) {
    UiBox *container = ui_box_str(0, id) {
        I64 h = time->hours;
        I64 m = time->minutes;
        I64 s = time->seconds;
        I64 ms = time->mseconds;

        F32 r = ui_config_get_vec4(UI_CONFIG_RADIUS_1).x;
        ui_style_rule(".int_picker #text_box") ui_style_vec4(UI_RADIUS, vec4(0, 0, 0, 0));
        ui_style_rule(".int_picker:first #text_box") ui_style_vec4(UI_RADIUS, vec4(0, r, 0, r));
        ui_style_rule(".int_picker:last #text_box") ui_style_vec4(UI_RADIUS, vec4(r, 0, r, 0));

        ui_int_picker(str("hours"), &h, 0, (mode==TIME_PICKER_ALARM ? 23 : INT64_MAX), (mode==TIME_PICKER_ALARM ? 2 : 4));
        ui_int_picker(str("minutes"), &m, 0, 59, 2);
        if (mode == TIME_PICKER_HMS || mode == TIME_PICKER_HMSM) ui_int_picker(str("seconds"), &s, 0, 59, 2);
        if (mode == TIME_PICKER_HMSM) ui_int_picker(str("mseconds"), &ms, 0, 999, 3);

        time->hours = h;
        time->minutes = m;
        time->seconds = s;
        time->mseconds = ms;
    }

    return container;
}

UiBox *ui_slider_str (String label, F32 *val) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, label) {
        ui_tag("slider");
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 20, 1});
        ui_style_f32(UI_EDGE_SOFTNESS, 0);
        ui_style_f32(UI_SPACING, 0);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);

        ui_style_rule(".focus") {
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
        }

        if (container->signals.focused && (ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == KEY_LEFT)) {
            *val -= .1;
            *val = clamp(*val, 0, 1);
            ui_eat_event();
        }

        if (container->signals.focused && (ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == KEY_RIGHT)) {
            *val += .1;
            *val = clamp(*val, 0, 1);
            ui_eat_event();
        }

        if (container->signals.pressed) {
            *val = (ui->mouse.x - container->rect.x) / container->rect.w;
            *val = clamp(*val, 0, 1);
        }

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL)) {
            *val = *val - (10*ui->event->y) / container->rect.w;
            *val = clamp(*val, 0, 1);
            ui_eat_event();
        }

        ui_box(UI_BOX_CLICK_THROUGH, "slider_track") {
            ui_style_f32(UI_FLOAT_X, 0);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 4, 0});
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1);
            ui_style_f32(UI_EDGE_SOFTNESS, 0);

            ui_box(UI_BOX_CLICK_THROUGH, "slider_track_fill") {
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, *val, 0});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
                ui_style_from_config(UI_BG_COLOR, UI_CONFIG_MAGENTA_1);
                ui_style_f32(UI_EDGE_SOFTNESS, 0);
            }
        }

        F32 knob_size = max(8, container->rect.h - 8);

        ui_box(UI_BOX_CLICK_THROUGH|UI_BOX_INVISIBLE, "slider_spacer") {
            F32 spacer_width = max(0, *val - knob_size/(2*max(knob_size, container->rect.w)));
            assert_dbg(spacer_width <= 1.0);
            assert_dbg(spacer_width >= 0.0);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, spacer_width, 0});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 2, 0});
        }

        ui_box(UI_BOX_CLICK_THROUGH, "slider_knob") {
            ui_style_f32(UI_EDGE_SOFTNESS, 1.3);
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_SLIDER_KNOB);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, knob_size, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, knob_size, 1});
            ui_style_vec4(UI_RADIUS, vec4(knob_size/2, knob_size/2, knob_size/2, knob_size/2));
        }
    }

    return container;
}

UiBox *ui_slider (CString label, F32 *val) {
    return ui_slider_str(str(label), val);
}

// Note that the grid will only display correctly as long as
// it's size is set to UI_SIZE_PIXELS or UI_SIZE_PCT_PARENT.
// If it has a downwards dependent size (UI_SIZE_CHILDREN_SUM)
// it will just collapse.
//
// The children of the grid must only be grid_cells:
//
//     ui_grid() {
//         ui_grid_cell(){}
//         ui_grid_cell(){}
//         ui_grid_cell(){}
//     }
//
UiBox *ui_grid_push (String label) {
    UiBox *grid = ui_box_push_str(0, label);
    ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    return grid;
}

Void ui_grid_pop () {
    UiBox *grid = array_get_last(&ui->box_stack);

    F32 rows = 0;
    F32 cols = 0;

    array_iter (cell, &grid->children) {
        Vec4 coords = *cast(Vec4*, cell->scratch);
        if ((coords.x + coords.z) > rows) rows = coords.x + coords.z;
        if ((coords.y + coords.w) > cols) cols = coords.y + coords.w;
    }

    F32 cell_width  = floor(grid->rect.w / rows);
    F32 cell_height = floor(grid->rect.h / cols);
    F32 spacing     = grid->style.spacing;

    array_iter (cell, &grid->children) {
        Vec4 coords = *cast(Vec4*, cell->scratch);
        cell->next_style.floating[0] = coords.x * (cell_width + spacing);
        cell->next_style.floating[1] = coords.y * (cell_height + spacing);
        cell->next_style.size.width  = (UiSize){UI_SIZE_PIXELS, coords.z * cell_width, 1};
        cell->next_style.size.height = (UiSize){UI_SIZE_PIXELS, coords.w * cell_height, 1};
    }

    ui_pop_parent();
}

// The unit of measurement for the x/y/w/h parameters is basic cells.
// That is, think of the grid as made up of basic cells. This function
// constructs super cells by defining on which basic cell they start,
// and how many basic cells they cover.
UiBox *ui_grid_cell_push (F32 x, F32 y, F32 w, F32 h) {
    UiBox *cell = ui_box_push_fmt(0, "grid_cell_%f_%f", x, y);
    ui_tag("grid_cell");
    ui_style_f32(UI_FLOAT_X, 0);
    ui_style_f32(UI_FLOAT_Y, 0);

    Vec4 *coords = mem_new(ui->frame_mem, Vec4);
    coords->x = x;
    coords->y = y;
    coords->z = w;
    coords->w = h;
    cell->scratch = cast(U64, coords);

    return cell;
}

Void ui_grid_cell_pop () {
    ui_pop_parent();
}

istruct (SatValPicker) {
    F32 hue;
    F32 sat;
    F32 val;
};

static Void draw_color_sat_val_picker (UiBox *box) {
    SatValPicker *info = cast(SatValPicker*, box->scratch);
    Rect *r = &box->rect;
    Vec4 c = hsva_to_rgba(vec4(info->hue, 1, 1, 1));
    Vec4 lc = c;

    dr_rect(
        .top_left     = r->top_left,
        .bottom_right = {r->x+r->w, r->y+r->h},
        .color        = lc,
        .color2       = {-1},
    );

    SliceVertex v = dr_rect(
        .top_left     = r->top_left,
        .bottom_right = {r->x+r->w, r->y+r->h},
        .color        = {1, 1, 1, 0},
        .color2       = {-1},
    );
    v.data[0].color = vec4(1, 1, 1, 1);
    v.data[1].color = vec4(1, 1, 1, 1);
    v.data[5].color = vec4(1, 1, 1, 1);

    dr_rect(
        .top_left     = r->top_left,
        .bottom_right = {r->x+r->w, r->y+r->h},
        .color        = {0, 0, 0, 0},
        .color2       = {0, 0, 0, 1},
    );

    F32 half = ui_config_get_font(UI_CONFIG_FONT_NORMAL)->size;
    Vec2 center = {
        box->rect.x + (info->sat * box->rect.w),
        box->rect.y + (1 - info->val) * box->rect.h
    };
    dr_rect(
        .edge_softness = 1,
        .radius        = { half, half, half, half },
        .top_left      = { center.x-half, center.y-half },
        .bottom_right  = { center.x+half, center.y+half },
        .color         = hsva_to_rgba(vec4(info->hue, info->sat, info->val, 1)),
        .color2        = {-1},
        .border_color  = {1, 1, 1, 1},
        .border_widths = {2, 2, 2, 2},
    );
}

UiBox *ui_color_sat_val_picker (String id, F32 hue, F32 *sat, F32 *val) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, id) {
        SatValPicker *data = mem_new(ui->frame_mem, SatValPicker);
        data->hue = hue;
        data->sat = *sat;
        data->val = *val;
        container->scratch = cast(U64, data);
        container->draw_fn = draw_color_sat_val_picker;
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 200, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 200, 0});

        if (container->signals.clicked || (container->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE)) {
            *sat = (ui->mouse.x - container->rect.x) / container->rect.w;
            *sat = clamp(*sat, 0, 1);
            *val = 1 - (ui->mouse.y - container->rect.y) / container->rect.h;
            *val = clamp(*val, 0, 1);
        }
    }

    return container;
}

static Void draw_color_hue_picker (UiBox *box) {
    F32 *hue = cast(F32*, box->scratch);
    F32 segment = box->rect.h / 6;
    Rect r = box->rect;
    r.h = segment;

    for (U64 i = 0; i < 6; ++i) {
        Vec4 col1 = hsva_to_rgba(vec4(cast(F32,i)/6, 1, 1, 1));
        Vec4 col2 = hsva_to_rgba(vec4(cast(F32, i+1)/6, 1, 1, 1));
        dr_rect(
            .top_left     = r.top_left,
            .bottom_right = {r.x+r.w, r.y+r.h},
            .color        = col1,
            .color2       = col2,
        );
        r.y += segment;
    }

    F32 half = ui_config_get_font(UI_CONFIG_FONT_NORMAL)->size;
    Vec2 center = {
        box->rect.x + box->rect.w/2,
        box->rect.y + *hue * box->rect.h,
    };
    dr_rect(
        .edge_softness = 1,
        .radius        = { half, half, half, half },
        .top_left      = { center.x-half, center.y-half },
        .bottom_right  = { center.x+half, center.y+half },
        .color         = hsva_to_rgba(vec4(*hue, 1, 1, 1)),
        .color2        = {-1},
        .border_color  = {1, 1, 1, 1},
        .border_widths = {2, 2, 2, 2},
    );
}

UiBox *ui_color_hue_picker (String id, F32 *hue) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, id) {
        container->draw_fn = draw_color_hue_picker;
        container->scratch = cast(U64, hue);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 20, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 200, 0});

        if (container->signals.clicked || (container->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE)) {
            *hue = (ui->mouse.y - container->rect.y) / container->rect.h;
            *hue = clamp(*hue, 0, 1);
        }
    }

    return container;
}

static Void draw_color_alpha_picker (UiBox *box) {
    F32 *alpha = cast(F32*, box->scratch);
    Rect *r = &box->rect;

    dr_rect(
        .top_left     = r->top_left,
        .bottom_right = {r->x+r->w, r->y+r->h},
        .color        = {1, 1, 1, 1},
        .color2       = {0, 0, 0, 1},
    );

    F32 half = ui_config_get_font(UI_CONFIG_FONT_NORMAL)->size;
    Vec2 center = {
        box->rect.x + box->rect.w/2,
        box->rect.y + (1 - *alpha) * box->rect.h,
    };
    dr_rect(
        .edge_softness = 1,
        .radius        = { half, half, half, half },
        .top_left      = { center.x-half, center.y-half },
        .bottom_right  = { center.x+half, center.y+half },
        .color         = { *alpha, *alpha, *alpha, 1 },
        .color2        = {-1},
        .border_color  = {1, 1, 1, 1},
        .border_widths = {2, 2, 2, 2},
    );
}

UiBox *ui_color_alpha_picker (String id, F32 *alpha) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, id) {
        container->draw_fn = draw_color_alpha_picker;
        container->scratch = cast(U64, alpha);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 20, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 200, 0});

        if (container->signals.clicked || (container->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE)) {
            *alpha = (ui->mouse.y - container->rect.y) / container->rect.h;
            *alpha = 1 - clamp(*alpha, 0, 1);
        }
    }

    return container;
}

istruct (UiColorPicker) {
    Mem *mem;
    F32 h, s, v, a;
    Bool valid;
    Buf *buf;
};

UiBox *ui_color_picker (String id, UiColorPickerMode mode, F32 *h, F32 *s, F32 *v, F32 *a) {
    UiBox *container = ui_box_str(0, id) {
        UiColorPicker *info = ui_get_box_data(container, sizeof(UiColorPicker), sizeof(UiColorPicker));
        if (! info->buf) info->buf = buf_new(info->mem, str(""));

        if (container->start_frame == ui->frame || info->h != *h || info->s != *s || info->v != *v || info->a != *a) {
            buf_clear(info->buf);
            info->valid = true;
            info->h = *h;
            info->s = *s;
            info->v = *v;
            info->a = *a;

            switch (mode) {
            case COLOR_PICKER_HEX: {
                Vec4 c = hsva_to_rgba(vec4(*h, *s, *v, *a));
                String str = astr_fmt(ui->frame_mem, "#%02x%02x%02x%02x", cast(U32, round(c.x*255)), cast(U32, round(c.y*255)), cast(U32, round(c.z*255)), cast(U32, round(c.w*255)));
                buf_insert(info->buf, 0, str);
            } break;

            case COLOR_PICKER_HSVA: {
                String str = astr_fmt(ui->frame_mem, "%u, %u, %u, %u", cast(U32, round(*h*255)), cast(U32, round(*s*255)), cast(U32, round(*v*255)), cast(U32, round(*a*255)));
                buf_insert(info->buf, 0, str);
            } break;

            case COLOR_PICKER_RGBA: {
                Vec4 c = hsva_to_rgba(vec4(*h, *s, *v, *a));
                String str = astr_fmt(ui->frame_mem, "%u, %u, %u, %u", cast(U32, round(c.x*255)), cast(U32, round(c.y*255)), cast(U32, round(c.z*255)), cast(U32, round(c.w*255)));
                buf_insert(info->buf, 0, str);
            } break;
            }
        }

        EventTag event_tag = ui->event->tag;
        UiBox *entry = ui_entry(str("entry"), info->buf, 18, str(""));
        container->next_style.size.width.strictness = 1;

        if (event_tag != ui->event->tag) {
            String text = buf_get_str(info->buf, ui->frame_mem);
            Vec4 hsva = {};
            info->valid = true;

            switch (mode) {
            case COLOR_PICKER_HEX: {
                info->valid = (text.count == 9);
                if (info->valid && text.data[0] != '#') info->valid = false;
                for (U64 i = 0; i < 4; ++i) {
                    if (! info->valid) break;
                    String token = str_slice(text, 1+2*i, 2);
                    U64 v;
                    info->valid = str_to_u64(cstr(ui->frame_mem, token), &v, 16);
                    if (info->valid && v > 255) info->valid = false;
                    if (info->valid) hsva.v[i] = cast(F32, v) / 255.f;
                }
            } break;

            case COLOR_PICKER_HSVA:
            case COLOR_PICKER_RGBA: {
                ArrayString tokens;
                array_init(&tokens, ui->frame_mem);
                str_split(text, str(", "), 0, 0, &tokens);
                if (tokens.count != 4) info->valid = false;
                array_iter (token, &tokens) {
                    if (! info->valid) break;
                    U64 v;
                    info->valid = str_to_u64(cstr(ui->frame_mem, token), &v, 10);
                    if (info->valid && v > 255) info->valid = false;
                    if (info->valid) hsva.v[ARRAY_IDX] = cast(F32, v) / 255.f;
                }
            } break;
            }

            if (info->valid) {
                if (mode != COLOR_PICKER_HSVA) hsva = rgba_to_hsva(hsva);
                *h = hsva.x;
                *s = hsva.y;
                *v = hsva.z;
                *a = hsva.w;
            }
        }

        if (! info->valid) {
            UiBox *inner = array_get(&entry->children, 0);
            ui_style_box_from_config(inner, UI_TEXT_COLOR, UI_CONFIG_RED_TEXT);
        }
    }

    return container;
}

UiBox *ui_color_picker_button (String id, F32 *h, F32 *s, F32 *v, F32 *a) {
    UiBox *button = ui_button(id) {
        Bool popup_shown = button->scratch;

        if (popup_shown || button->signals.clicked) {
            ui_tag_box(button, "press");

            ui_popup("popup", &popup_shown, false, button) {
                ui_box(0, "color_view") {
                    ui_style_vec2(UI_PADDING, vec2(16.0, 16));
                    ui_style_f32(UI_SPACING, 10.0);
                    ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
                    ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 250, 1});
                    ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 350, 1});

                    ui_box(0, "graphical_pickers") {
                        ui_style_f32(UI_SPACING, 8.0);
                        ui_color_sat_val_picker(str("sat_val"), *h, s, v);
                        ui_color_hue_picker(str("hue"), h);
                        ui_color_alpha_picker(str("alpha"), a);
                    }

                    ui_box(UI_BOX_INVISIBLE, "spacer") ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 10, 1});

                    ui_box(0, "hex_picker") {
                        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
                        ui_label(0, "label", str("HEX: "));
                        ui_hspacer();
                        ui_color_picker(str("picker"), COLOR_PICKER_HEX, h, s, v, a);
                    }

                    ui_box(0, "rgba_picker") {
                        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
                        ui_label(0, "label", str("RGBA: "));
                        ui_hspacer();
                        ui_color_picker(str("picker"), COLOR_PICKER_RGBA, h, s, v, a);
                    }

                    ui_box(0, "hsva_picker") {
                        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
                        ui_label(0, "label", str("HSVA: "));
                        ui_hspacer();
                        ui_color_picker(str("picker"), COLOR_PICKER_HSVA, h, s, v, a);
                    }
                }
            }

            button->scratch = popup_shown;
        }

        ui_box(UI_BOX_CLICK_THROUGH, "color") {
            ui_style_vec4(UI_BG_COLOR, hsva_to_rgba(vec4(*h, *s, *v, *a)));
            Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 3*font->width, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, font->height, 1});
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
            ui_style_f32(UI_EDGE_SOFTNESS, 0);
        }
    }

    return button;
}

static Void size_ym_picker (UiBox *ym_picker, U64 axis) {
    UiBox *day_titles = cast(UiBox*, ym_picker->scratch);
    ym_picker->rect.size[axis] = day_titles->rect.size[axis];
}

UiBox *ui_date_picker (String id, Date *date) {
    tmem_new(tm);

    UiBox *container = ui_box_str(0, id) {
        ui_style_from_config(UI_PADDING, UI_CONFIG_PADDING_1);
        ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);

        F32 spacing = 2;
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);

        ui_style_rule(".icon_button") {
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);
            ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 3*font->size, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 3*font->size, 1});
            ui_style_f32(UI_EDGE_SOFTNESS, 1);
            F32 r = 3*font->size/2;
            ui_style_vec4(UI_RADIUS, vec4(r, r, r, r));
        }

        ui_style_rule(".icon_button.hover") ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_SELECTION);
        ui_style_rule(".icon_button.hover *") ui_style_from_config(UI_TEXT_COLOR, UI_CONFIG_TEXT_SELECTION);

        ui_style_rule(".icon_button.focus") {
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
        }

        UiBox *ym_picker = ui_box(0, "year_and_month_pickers") {
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CUSTOM, 0, 1});
            ym_picker->size_fn = size_ym_picker;

            ui_box(0, "year_picker") {
                ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CHILDREN_SUM, 1, 1});
                ui_style_f32(UI_SPACING, 4);

                UiBox *left_btn  = ui_icon_button(0, "left_btn", 16, UI_ICON_PAN_LEFT);
                UiBox *label     = ui_label(0, "label", astr_fmt(tm, "%u", date->year));
                UiBox *right_btn = ui_icon_button(0, "right_btn", 16, UI_ICON_PAN_RIGHT);

                ui_style_box_from_config(label, UI_FONT, UI_CONFIG_FONT_MONO);

                if (left_btn->signals.clicked)  date->year = (date->year == 0) ? 0 : date->year - 1;
                if (right_btn->signals.clicked) date->year = (date->year == UINT32_MAX) ? UINT32_MAX : date->year + 1;
            }

            ui_box(0, "month_picker") {
                ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
                ui_style_u32(UI_ALIGN_X, UI_ALIGN_END);
                ui_style_f32(UI_SPACING, 4);
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});

                UiBox *left_btn  = ui_icon_button(0, "left_btn", 16, UI_ICON_PAN_LEFT);
                UiBox *label     = ui_label(0, "label", astr_fmt(tm, "%02u", date->month));
                UiBox *right_btn = ui_icon_button(0, "right_btn", 16, UI_ICON_PAN_RIGHT);

                ui_style_box_from_config(label, UI_FONT, UI_CONFIG_FONT_MONO);

                if (left_btn->signals.clicked)  date->month = (date->month == 1) ? 12 : date->month - 1;
                if (right_btn->signals.clicked) date->month = (date->month == 12) ? 1 : date->month + 1;
            }
        }

        UiBox *day_titles = ui_box(0, "day_titles") {
            ui_style_f32(UI_SPACING, spacing);

            CString titles[] = {"S", "M", "T", "W", "T", "F", "S"};
            for (U64 i = 0; i < 7; ++i) {
                ui_box_fmt(0, "title%lu", i) {
                    ui_tag("icon_button");
                    UiBox *label = ui_label(0, "label", str(titles[i]));
                    ui_style_box_from_config(label, UI_FONT, UI_CONFIG_FONT_MONO);
                }
            }
        }

        ym_picker->scratch = cast(U64, day_titles);

        ui_box(0, "day_picker") {
            ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
            ui_style_f32(UI_SPACING, spacing);

            U32 first_weekday = os_first_weekday(date->year, date->month);
            U32 days_in_this_month = os_days_in_month(date->year, date->month);
            if (first_weekday == 0) first_weekday = 7;

            U32 prev_month = (date->month == 1) ? 12 : date->month - 1;
            U32 prev_year  = (date->month == 1) ? date->year - 1 : date->year;
            U32 days_in_prev_month = os_days_in_month(prev_year, prev_month);

            U32 next_month = (date->month == 12) ? 1 : date->month + 1;
            U32 next_year  = (date->month == 12) ? date->year + 1 : date->year;

            U32 day = 1;
            U32 next_day = 1;

            for (U64 week = 0; week < 6; ++week) {
                ui_box_fmt(0, "row%lu", week) {
                    ui_style_f32(UI_SPACING, spacing);

                    for (U64 day_of_week = 0; day_of_week < 7; ++day_of_week) {
                        UiBox *cell = ui_box_fmt(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, "cell%lu", day_of_week) {
                            ui_tag("icon_button");

                            U32 cell_idx = week * 7 + day_of_week;
                            Date d;
                            Bool dim = false;

                            if (cell_idx < first_weekday) {
                                dim = true;
                                d = (Date){prev_year, prev_month, (days_in_prev_month - first_weekday + 1 + cell_idx)};
                            } else if (day <= days_in_this_month) {
                                d = (Date){date->year, date->month, day};
                                day++;
                            } else {
                                dim = true;
                                d = (Date){next_year, next_month, next_day};
                                next_day++;
                            }

                            Bool selected = (d.month == date->month && d.day == date->day);

                            UiBox *label = ui_label(0, "label", astr_fmt(tm, "%02u", d.day));
                            ui_style_box_from_config(label, UI_FONT, UI_CONFIG_FONT_MONO);

                            if (dim) {
                                ui_style_box_from_config(label, UI_TEXT_COLOR, UI_CONFIG_TEXT_COLOR_3);
                            }

                            if (selected) {
                                ui_style_box_from_config(label, UI_TEXT_COLOR, UI_CONFIG_TEXT_SELECTION);
                                ui_style_box_from_config(cell, UI_BG_COLOR, UI_CONFIG_BG_SELECTION);
                            }

                            if (cell->signals.clicked) *date = d;
                        }
                    }
                }
            }
        }
    }

    return container;
}

istruct (FilePickerSearchResult) {
    I64 score;
    Bool is_dir;
    String name;
    String full_path;
};

istruct (FilePicker) {
    Mem *mem;
    Buf *search;
    U64 search_version;
    ArrayString selections;
    Array(FilePickerSearchResult) search_results;
};

static Int cmp_file_picker_results (Void *A, Void *B) {
    FilePickerSearchResult *a = A;
    FilePickerSearchResult *b = B;
    return (a->score < b->score) ? 1 : (a->score == b->score) ? 0 : -1;
}

UiBox *ui_file_picker (String id, Buf *buf, Bool *shown, Bool multiple, Bool dir_only) {
    tmem_new(tm);

    UiBox *container = ui_box_str(0, id) {
        FilePicker *info = ui_get_box_data(container, sizeof(FilePicker), 1*KB);
        if (! info->search) {
            info->search = buf_new(info->mem, fs_get_current_working_dir(tm));
            array_init(&info->search_results, info->mem);
            array_init(&info->selections, info->mem);
        }

        ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
        ui_style_from_config(UI_SPACING, UI_CONFIG_SPACING_1);

        UiBox *ok_button;
        UiTextBoxInfo *search_text_box_info;

        ui_box(0, "header") {
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 1});
            ui_style_from_config(UI_SPACING, UI_CONFIG_SPACING_1);

            UiBox *entry = ui_entry(str("entry"), info->search, 64, str(""));
            UiBox *inner = array_get(&entry->children, 0);
            ui_style_box_size(inner, UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
            search_text_box_info = ui_get_box_data(inner, sizeof(UiTextBoxInfo), sizeof(UiTextBoxInfo));
            if (entry->start_frame == ui->frame) {
                ui_grab_focus(array_get(&inner->children, 0));
                ui_tbox_cursor_move_to_end(search_text_box_info, &search_text_box_info->cursor, true);
            }

            ok_button = ui_button_label_str(str("ok_button"), str("Ok"));
        }

        // Gather and sort results:
        if (info->search_version != buf_get_version(info->search)) {
            String search = buf_get_str(info->search, tm);
            String prefix = str_prefix_to_last(search, '/');
            String suffix = str_suffix_from_last(search, '/');
            FsIter *it = fs_iter_new(tm, prefix, false, dir_only);
            info->search_results.count = 0;
            while (fs_iter_next(it)) {
                I64 score = str_fuzzy_search(suffix, it->current_file_name, 0);
                if (score != INT64_MIN) array_push_lit(
                    &info->search_results,
                    .score=score,
                    .full_path=str_copy(info->mem, it->current_full_path.as_slice),
                    .name=str_copy(info->mem, it->current_file_name),
                    .is_dir=it->is_directory
                );
            }
            fs_iter_destroy(it);
            array_sort_cmp(&info->search_results, cmp_file_picker_results);
            info->search_version = buf_get_version(info->search);
        }

        ui_scroll_box("results") {
            ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);

            array_iter (r, &info->search_results, *) {
                UiBox *button = ui_button(astr_fmt(tm, "%lu", ARRAY_IDX)) {
                    ui_style_u32(UI_ALIGN_X, UI_ALIGN_START);
                    ui_style_vec4(UI_BG_COLOR, vec4(0, 0, 0, 0));
                    ui_style_vec4(UI_BG_COLOR2, vec4(-1, 0, 0, 0));
                    ui_style_f32(UI_OUTSET_SHADOW_WIDTH, 0);
                    ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
                    if (button->signals.hovered) ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_4);

                    U64 selected = array_find(&info->selections, str_match(IT, r->full_path));
                    if (selected != ARRAY_NIL_IDX) ui_tag_box(button, "press");

                    if (button->signals.clicked) {
                        if (selected != ARRAY_NIL_IDX) {
                            array_remove_fast(&info->selections, selected);
                        } else if (r->is_dir && !dir_only) {
                            ui_tbox_cursor_move_to_end(search_text_box_info, &search_text_box_info->cursor, true);
                            ui_tbox_cursor_insert(search_text_box_info, &search_text_box_info->cursor, r->name);
                            ui_tbox_cursor_insert(search_text_box_info, &search_text_box_info->cursor, str("/"));
                        } else {
                            array_push(&info->selections, r->full_path);
                        }
                    }

                    ui_box(0, "content") {
                        ui_style_from_config(UI_SPACING, UI_CONFIG_SPACING_1);
                        ui_icon(UI_BOX_CLICK_THROUGH, "icon", 16, r->is_dir ? UI_ICON_FOLDER : UI_ICON_FILE);
                        ui_label(UI_BOX_CLICK_THROUGH, "label", r->name);
                    }
                }
            }
        }

        // Autocompletion with tab:
        if (ui->event->tag == EVENT_KEY_PRESS && ui->event->key == KEY_TAB) {
            FilePickerSearchResult r = array_get(&info->search_results, 0);
            String search = buf_get_str(info->search, tm);
            String prefix = str_prefix_to_last(search, '/');
            String new_str = astr_fmt(tm, "%.*s/%.*s%s", STR(prefix), STR(r.name), r.is_dir ? "/" : "");
            buf_clear(info->search);
            ui_tbox_cursor_insert(search_text_box_info, &search_text_box_info->cursor, new_str);
            ui_eat_event();
        }

        // Commit selections:
        if (ok_button->signals.clicked ||
            (!multiple && info->selections.count) ||
            (ui->event->tag == EVENT_KEY_PRESS && ui->event->key == KEY_RETURN)
        ) {
            ui_eat_event();
            *shown = false;
            buf_clear(buf);
            if (info->selections.count) {
                array_iter (sel, &info->selections) {
                    buf_insert(buf, buf_get_count(buf), sel);
                    if (! ARRAY_ITER_DONE) buf_insert(buf, buf_get_count(buf), str(" | "));
                }
            } else {
                buf_insert(buf, 0, buf_get_str(info->search, tm));
            }
        }
    }

    return container;
}

UiBox *ui_file_picker_entry (String id, Buf *buf, Bool multiple, Bool dir_only) {
    UiBox *container = ui_box_str(0, id) {
        ui_entry(str("entry"), buf, 25, str("Select files..."));
        UiBox *button = ui_button(str("button")) ui_icon(UI_BOX_CLICK_THROUGH, "icon", 16, UI_ICON_SEARCH);

        F32 r = ui_config_get_vec4(UI_CONFIG_RADIUS_1).x;
        ui_style_box_vec4(button, UI_RADIUS, vec4(r, 0, r, 0));
        ui_style_rule("#entry #text_box") ui_style_vec4(UI_RADIUS, vec4(0, r, 0, r));

        Bool shown = button->scratch;
        if (shown || button->signals.clicked) {
            ui_modal("modal", &shown) {
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, .5, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, .8, 1});
                ui_style_u32(UI_ANIMATION, UI_MASK_BG_COLOR);
                ui_style_from_config(UI_ANIMATION_TIME, UI_CONFIG_ANIMATION_TIME_3);
                ui_file_picker(str("file_picker"), buf, &shown, multiple, dir_only);
            }

            button->scratch = shown;
        }
    }

    return container;
}
