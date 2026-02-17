#include "vendor/glad/glad.h"
#include <SDL3/SDL.h>
#include "ui/ui_widgets.h"
#include "base/string.h"

istruct (UiTextBox);
static Vec2 text_box_cursor_to_coord (UiTextBox *info, UiBox *box, BufCursor *pos);
static BufCursor text_box_coord_to_cursor (UiTextBox *info, UiBox *box, Vec2 coord);

UiBox *ui_hspacer () {
    UiBox *box = ui_box(UI_BOX_INVISIBLE, "hspacer") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
    return box;
}

UiBox *ui_vspacer () {
    UiBox *box = ui_box(UI_BOX_INVISIBLE, "vspacer") { ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
    return box;
}

static Void size_label (UiBox *box, U64 axis) {
    // Sizing done in the draw_label function.
}

static Void draw_label (UiBox *box) {
    if (! ui_set_font(box)) return;

    tmem_new(tm);

    glBindTexture(GL_TEXTURE_2D, ui->font->atlas_texture);

    Bool first_frame     = box->start_frame == ui->frame;
    String text          = str(cast(CString, box->scratch));
    F32 x                = round(box->rect.x + box->style.padding.x);
    F32 y                = round(box->rect.y + box->rect.h - box->style.padding.y);
    U32 line_width       = 0;
    F32 descent          = cast(F32, ui->font->descent);
    F32 width            = cast(F32, ui->font->width);
    F32 x_pos            = x;
    SliceGlyphInfo infos = font_get_glyph_infos(ui->font, tm, text);

    array_iter (info, &infos, *) {
        AtlasSlot *slot = font_get_atlas_slot(ui->font, info);
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
        if (ARRAY_ITER_DONE) line_width = ui->font->is_mono ? (x_pos - x) : (info->x + slot->bearing_x + info->x_advance);
    }

    box->rect.w = line_width + 2*box->style.padding.x;
    box->rect.h = ui->font->height + 2*box->style.padding.y;
}

UiBox *ui_label (CString id, String label) {
    UiBox *box = ui_box_str(UI_BOX_CLICK_THROUGH, str(id)) {
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

UiBox *ui_icon (CString id, U32 size, U32 icon) {
    UiBox *label = ui_label(id, str_utf32_to_utf8(ui->frame_mem, icon));
    ui_style_box_from_config(label, UI_FONT, UI_CONFIG_FONT_ICONS);
    ui_style_box_f32(label, UI_FONT_SIZE, size);
    return label;
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
            ui_icon("mark", 16, get_icon(ICON_CHECK));
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
    Image *image;
    Bool blur;
    Vec4 tint;
    F32 pref_width;
};

static Void draw_image (UiBox *box) {
    Auto info = cast(UiImage *, box->scratch);
    dr_flush_vertices();
    glBindTexture(GL_TEXTURE_2D, info->image->texture);
    dr_rect(
        .top_left          = box->rect.top_left,
        .bottom_right      = {box->rect.x + box->rect.w, box->rect.y + box->rect.h},
        .radius            = box->style.radius,
        .texture_rect      = {0, 0, info->image->width, info->image->height},
        .text_color        = (info->tint.w > 0) ? info->tint : vec4(1, 1, 1, 1),
        .text_is_grayscale = (info->tint.w > 0) ? 1 : 0,
    );
}

UiBox *ui_image (CString id, Image *image, Bool blur, Vec4 tint, F32 pref_width) {
    UiBox *img = ui_box(UI_BOX_INVISIBLE, id) {
        img->draw_fn = draw_image;
        UiImage *info = mem_new(ui->frame_mem, UiImage);
        info->image = image;
        info->blur = blur;
        info->tint = tint;
        info->pref_width = pref_width;
        img->scratch = cast(U64, info);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, info->pref_width, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, round(image->height * (info->pref_width / image->width)), 1});
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

UiBox *ui_button_str (String id, String label) {
    UiBox *button = ui_box_str(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, id) {
        ui_tag("button");
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
        ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_3);
        ui_style_from_config(UI_BG_COLOR2, UI_CONFIG_FG_4);
        ui_style_from_config(UI_RADIUS, UI_CONFIG_RADIUS_1);
        ui_style_from_config(UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
        ui_style_from_config(UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
        ui_style_from_config(UI_PADDING, UI_CONFIG_PADDING_1);
        ui_style_vec2(UI_SHADOW_OFFSETS, vec2(0, -1));

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

        if (button->signals.hovered) {
            ui_push_clip(button, true);
            ui_box(UI_BOX_CLICK_THROUGH, "button_highlight") {
                F32 s = button->rect.h/8;
                ui_style_f32(UI_EDGE_SOFTNESS, 60);
                ui_style_vec4(UI_RADIUS, vec4(s, s, s, s));
                ui_style_f32(UI_FLOAT_X, ui->mouse.x - button->rect.x - s);
                ui_style_f32(UI_FLOAT_Y, ui->mouse.y - button->rect.y - s);
                ui_style_from_config(UI_BG_COLOR, UI_CONFIG_HIGHLIGHT);
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 2*s, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 2*s, 1});
            }
            ui_pop_clip();
        }

        UiBox *label_box = ui_label("button_label", label);
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);
        ui_style_box_font(label_box, UI_FONT, font);
        ui_style_box_f32(label_box, UI_FONT_SIZE, font->size);
    }

    return button;
}

UiBox *ui_button (CString id) {
    return ui_button_str(str(id), str(id));
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
    if (contains_focused && ui->event->tag == EVENT_KEY_PRESS && ui->event->key == SDLK_TAB) {
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

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL) && ui_is_key_pressed(SDLK_LCTRL)) {
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

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL) && !ui_is_key_pressed(SDLK_LCTRL)) {
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

    array_iter(child, &popup->children) {
        if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT) cycle = true;
        size += child->rect.size[axis];
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
    if ((ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_ESCAPE)) *shown = false;
    if (overlay->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) *shown = false;

    UiBox *popup = ui_scroll_box_push(str("popup"));
    popup->size_fn = size_popup;
    UiPopup *info = mem_new(ui->frame_mem, UiPopup);
    info->sideways = sideways;
    info->shown = shown;
    info->anchor = anchor;
    popup->scratch = cast(U64, info);
    array_push_lit(&ui->deferred_layout_fns, layout_popup, popup);
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

    array_iter(child, &modal->children) {
        if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT) cycle = true;
        size += child->rect.size[axis];
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
    if ((ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_ESCAPE)) *shown = false;
    if (overlay->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) *shown = false;

    UiBox *modal = ui_scroll_box_push(str("modal"));
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
    ui_scroll_box_pop();
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

static Void text_box_draw_line (UiTextBox *info, UiBox *box, U32 line_idx, String text, Vec4 color, F32 x, F32 y) {
    tmem_new(tm);
    glBindTexture(GL_TEXTURE_2D, ui->font->atlas_texture);

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

                if (scroll_y && !ui_is_key_pressed(SDLK_LSHIFT)) {
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
            case SDLK_DELETE:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    buf_cursor_move_right_word(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                } else {
                    buf_cursor_move_right(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                }
                ui_eat_event();
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                break;
            case SDLK_W:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    buf_cursor_move_left_word(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                    text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                    ui_eat_event();
                }
                break;
            case SDLK_A:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    buf_cursor_move_to_end(info->buf, &info->cursor, true);
                    buf_cursor_move_to_start(info->buf, &info->cursor, false);
                    text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                    ui_eat_event();
                }
                break;
            case SDLK_V:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    String text = win_get_clipboard_text(ui->frame_mem);
                    buf_insert(info->buf, &info->cursor, text);
                }
                break;
            case SDLK_X:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    String text = buf_get_selection(info->buf, &info->cursor);
                    if (text.count) {
                        win_set_clipboard_text(text);
                        buf_delete(info->buf, &info->cursor);
                    }
                }
                break;
            case SDLK_C:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    String text = buf_get_selection(info->buf, &info->cursor);
                    if (text.count) win_set_clipboard_text(text);
                }
                break;
            case SDLK_RETURN:
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
            case SDLK_BACKSPACE:
                if (info->cursor.byte_offset == info->cursor.selection_offset) buf_cursor_move_left(info->buf, &info->cursor, false);
                buf_delete(info->buf, &info->cursor);
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_LEFT:
                buf_cursor_move_left(info->buf, &info->cursor, !(ui->event->mods & SDL_KMOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_RIGHT:
                buf_cursor_move_right(info->buf, &info->cursor, !(ui->event->mods & SDL_KMOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_UP:
                buf_cursor_move_up(info->buf, &info->cursor, !(ui->event->mods & SDL_KMOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_DOWN:
                buf_cursor_move_down(info->buf, &info->cursor, !(ui->event->mods & SDL_KMOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            }
        }

        if (text_box->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) {
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

UiBox *ui_entry (String id, Buf *buf, F32 width_in_chars, String hint) {
    UiBox *container = ui_box_str(UI_BOX_INVISIBLE, id) {
        UiBox *text_box = ui_text_box(str("text"), buf, true);
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
            UiBox *h = ui_label("hint", hint);
            UiBox *inner_text = array_get(&text_box->children, 0);
            ui_style_box_f32(h, UI_FLOAT_X, inner_text->rect.x - container->rect.x);
            ui_style_box_f32(h, UI_FLOAT_Y, inner_text->rect.y - container->rect.y);
            ui_style_box_font(h, UI_FONT, text_box->next_style.font);
            ui_style_box_f32(h, UI_FONT_SIZE, text_box->next_style.font_size);
            ui_style_box_from_config(h, UI_TEXT_COLOR, UI_CONFIG_TEXT_COLOR_2);
        }
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
        UiIntPicker *info = ui_get_box_data(container, sizeof(UiIntPicker), sizeof(UiIntPicker));
        if (! info->buf) info->buf = buf_new(info->mem, str(""));

        if (container->start_frame == ui->frame || info->val != *val) {
            String str = astr_fmt(ui->frame_mem, "%li", *val);
            buf_clear(info->buf);
            buf_insert(info->buf, &(BufCursor){}, str);
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
            ui_tooltip("tooltip") ui_label("label", astr_fmt(ui->frame_mem, "Integer in range [%li, %li].", min, max));

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

        if (container->signals.focused && (ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_LEFT)) {
            *val -= .1;
            *val = clamp(*val, 0, 1);
            ui_eat_event();
        }

        if (container->signals.focused && (ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_RIGHT)) {
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

    array_iter (cell, &grid->children) {
        Vec4 coords = *cast(Vec4*, cell->scratch);
        cell->next_style.floating[0] = coords.x * cell_width;
        cell->next_style.floating[1] = coords.y * cell_height;
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
    ui_style_f32(UI_FLOAT_X, 0);
    ui_style_f32(UI_FLOAT_Y, 0);
    ui_style_from_config(UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
    ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
    ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
    ui_style_f32(UI_EDGE_SOFTNESS, 0);

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
                buf_insert(info->buf, &(BufCursor){}, str);
            } break;

            case COLOR_PICKER_HSVA: {
                String str = astr_fmt(ui->frame_mem, "%u, %u, %u, %u", cast(U32, round(*h*255)), cast(U32, round(*s*255)), cast(U32, round(*v*255)), cast(U32, round(*a*255)));
                buf_insert(info->buf, &(BufCursor){}, str);
            } break;

            case COLOR_PICKER_RGBA: {
                Vec4 c = hsva_to_rgba(vec4(*h, *s, *v, *a));
                String str = astr_fmt(ui->frame_mem, "%u, %u, %u, %u", cast(U32, round(c.x*255)), cast(U32, round(c.y*255)), cast(U32, round(c.z*255)), cast(U32, round(c.w*255)));
                buf_insert(info->buf, &(BufCursor){}, str);
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
