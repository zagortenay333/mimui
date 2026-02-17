#pragma once

#include "base/core.h"
#include "base/log.h"
#include "base/math.h"
#include "base/string.h"
#include "os/time.h"
#include "base/map.h"
#include "os/fs.h"

ienum (EventTag, U8) {
    EVENT_DUMMY,
    EVENT_EATEN,
    EVENT_WINDOW_SIZE,
    EVENT_MOUSE_MOVE,
    EVENT_SCROLL,
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_TEXT_INPUT,
};

istruct (Event) {
    EventTag tag;
    F64 x;
    F64 y;
    Int key;
    Int mods;
    Int scancode;
    String text;
};

istruct (Rect) {
    union { struct { F32 x, y; }; Vec2 top_left; };
    union { struct { F32 w, h; }; F32 size[2]; };
};

istruct (RectAttributes) {
    Vec4 color;
    Vec4 color2; // If x = -1, no gradient.
    Vec2 top_left;
    Vec2 bottom_right;
    Vec4 radius;
    F32  edge_softness;
    Vec4 border_color;
    Vec4 border_widths;
    Vec4 inset_shadow_color;
    Vec4 outset_shadow_color;
    F32  outset_shadow_width;
    F32  inset_shadow_width;
    Vec2 shadow_offsets;
    Vec4 texture_rect;
    Vec4 text_color;
    F32 text_is_grayscale;
};

istruct (Vertex) {
    Vec2 position;
    Vec4 color;
    Vec2 top_left;
    Vec2 bottom_right;
    Vec4 radius;
    F32 edge_softness;
    Vec4 border_color;
    Vec4 border_widths;
    Vec4 inset_shadow_color;
    Vec4 outset_shadow_color;
    F32 outset_shadow_width;
    F32 inset_shadow_width;
    Vec2 shadow_offsets;
    Vec2 uv;
    Vec4 text_color;
    F32 text_is_grayscale;
};

istruct (Image) {
    U32 texture;
    F32 width;
    F32 height;
};

array_typedef(Vertex, Vertex);
array_typedef(Event, Event);

Void        win_init               ();
Void        win_run                (Void (*)(F64 dt));
SliceEvent *win_get_events         ();
Void        win_set_clipboard_text (String);
String      win_get_clipboard_text (Mem *);
Vec2        win_get_size           ();

Image       dr_image            (CString filepath, Bool flip);
Void        dr_flush_vertices   ();
Vertex     *dr_reserve_vertices (U32 n);
SliceVertex dr_rect_fn          (RectAttributes *);
Void        dr_blur             (Rect, F32 strength, Vec4 corner_radius);

#define dr_rect(...)\
    dr_rect_fn(&(RectAttributes){__VA_ARGS__})
