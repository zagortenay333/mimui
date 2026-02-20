#pragma once

#include "base/core.h"
#include "ui/ui.h"
#include "buffer/buffer.h"

ienum (UiColorPickerMode, U8) {
    COLOR_PICKER_HEX,
    COLOR_PICKER_RGBA,
    COLOR_PICKER_HSVA,
};

UiBox *ui_hspacer              ();
UiBox *ui_vspacer              ();
UiBox *ui_label                (UiBoxFlags, CString id, String label);
UiBox *ui_icon                 (UiBoxFlags, CString id, U32 size, U32 icon);
UiBox *ui_checkbox             (CString id, Bool *val);
UiBox *ui_image                (CString id, Texture *, Bool blur, Vec4 tint, F32 pref_width);
UiBox *ui_toggle               (CString id, Bool *val);
UiBox *ui_button_push          (String id);
Void   ui_button_pop           ();
UiBox *ui_button_label_str     (String id, String label);
UiBox *ui_button_label         (CString id);
UiBox *ui_vscroll_bar          (String id, Rect rect, F32 ratio, F32 *val);
UiBox *ui_hscroll_bar          (String id, Rect rect, F32 ratio, F32 *val);
UiBox *ui_entry                (String id, Buf *buf, F32 width_in_chars, String hint);
UiBox *ui_shortcut_picker      (String id, Key *, KeyMod *);
UiBox *ui_int_picker           (String id, I64 *val, I64 min, I64 max, U8 width_in_chars);
UiBox *ui_dropdown             (String id, U64 *selection, SliceString options);
UiBox *ui_slider_str           (String id, F32 *val);
UiBox *ui_slider               (CString id, F32 *val);
UiBox *ui_color_sat_val_picker (String id, F32 hue, F32 *sat, F32 *val);
UiBox *ui_color_hue_picker     (String id, F32 *hue);
UiBox *ui_color_alpha_picker   (String id, F32 *alpha);
UiBox *ui_color_picker         (String id, UiColorPickerMode mode, F32 *h, F32 *s, F32 *v, F32 *a);
UiBox *ui_grid_push            (String id);
Void   ui_grid_pop             ();
UiBox *ui_grid_cell_push       (F32 x, F32 y, F32 w, F32 h);
Void   ui_grid_cell_pop        ();
UiBox *ui_tooltip_push         (String id);
Void   ui_tooltip_pop          ();
UiBox *ui_modal_push           (String id, Bool *shown);
Void   ui_modal_pop            ();
UiBox *ui_popup_push           (String id, Bool *shown, Bool sideways, UiBox *anchor);
Void   ui_popup_pop            ();
UiBox *ui_scroll_box_push      (String id);
Void   ui_scroll_box_pop       ();
UiBox *ui_date_picker          (String id, Date *);

#define ui_grid(L)             ui_grid_push(str(L));                   if (cleanup(ui_grid_pop_) U8 _; 1)
#define ui_grid_cell(...)      ui_grid_cell_push(__VA_ARGS__);         if (cleanup(ui_grid_cell_pop_) U8 _; 1)
#define ui_tooltip(LABEL)      ui_tooltip_push(str(LABEL));            if (cleanup(ui_tooltip_pop_) U8 _; 1)
#define ui_modal(LABEL, SHOWN) ui_modal_push(str(LABEL), SHOWN);       if (cleanup(ui_modal_pop_) U8 _; 1)
#define ui_scroll_box(LABEL)   ui_scroll_box_push(str(LABEL));         if (cleanup(ui_scroll_box_pop_) U8 _; 1)
#define ui_popup(LABEL, ...)   ui_popup_push(str(LABEL), __VA_ARGS__); if (cleanup(ui_popup_pop_) U8 _; 1)
#define ui_button(LABEL)       ui_button_push(LABEL);                  if (cleanup(ui_button_pop_) U8 _; 1)

inl Void ui_grid_pop_       (Void *) { ui_grid_pop(); }
inl Void ui_grid_cell_pop_  (Void *) { ui_grid_cell_pop(); }
inl Void ui_tooltip_pop_    (Void *) { ui_tooltip_pop(); }
inl Void ui_modal_pop_      (Void *) { ui_modal_pop(); }
inl Void ui_popup_pop_      (Void *) { ui_popup_pop(); }
inl Void ui_scroll_box_pop_ (Void *) { ui_scroll_box_pop(); }
inl Void ui_button_pop_     (Void *) { ui_button_pop(); }
