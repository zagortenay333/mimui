#pragma once

#include "base/core.h"
#include "base/log.h"
#include "base/math.h"
#include "base/string.h"
#include "os/time.h"
#include "base/map.h"
#include "os/fs.h"

// =============================================================================
// Window api:
// =============================================================================
ienum (Key, U32) {
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_DEL,
    KEY_BACKSPACE,
    KEY_RETURN,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_TAB,
    KEY_ESC,

    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,

    KEY_MOUSE_LEFT,
    KEY_MOUSE_MIDDLE,
    KEY_MOUSE_RIGHT,

    KEY_UNKNOWN,
};

fenum (KeyMod, U8) {
    KEY_MOD_SHIFT = flag(0),
    KEY_MOD_CTRL  = flag(1),
    KEY_MOD_ALT   = flag(2),
};

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
    Key key;
    KeyMod mods;
    String text;
};

array_typedef(Event, Event);

Void        win_init               ();
Void        win_run                (Void (*)(F64 dt));
SliceEvent *win_get_events         ();
Void        win_set_clipboard_text (String);
String      win_get_clipboard_text (Mem *);
Vec2        win_get_size           ();

// =============================================================================
// Drawing api:
// =============================================================================
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

Image       dr_image            (CString filepath, Bool flip);
Void        dr_flush_vertices   ();
Vertex     *dr_reserve_vertices (U32 n);
SliceVertex dr_rect_fn          (RectAttributes *);
Void        dr_blur             (Rect, F32 strength, Vec4 corner_radius);
Void        dr_scissor          (Rect);
Void        dr_bind_texture     (U32);

#define dr_rect(...)\
    dr_rect_fn(&(RectAttributes){__VA_ARGS__})
