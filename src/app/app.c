#include "app/app.h"
#include "ui/ui.h"
#include "ui/ui_widgets.h"
#include "ui/ui_text_box.h"
#include "buffer/buffer.h"
#include "window/window.h"

istruct (App) {
    Buf *buf1;
    Buf *buf2;

    Bool modal_shown;
    Bool popup_shown;

    U32 view;

    F32 slider;
    Bool toggle;

    I64 intval;
    Texture image;

    F32 hue;
    F32 sat;
    F32 val;
    F32 alpha;
};

App *app;

static Void build_text_view () {
    UiBox *box = ui_text_box(str("text_box"), app->buf1, false);
    ui_style_box_size(box, UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});
    ui_style_box_size(box, UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_box_vec2(box, UI_PADDING, (Vec2){8, 8});
}

static Void build_clock_view () {
    ui_box(UI_BOX_CLICK_THROUGH, "clock_box") {
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CHILDREN_SUM, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 1, 0});
        ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);

        Time time = os_get_wall_time();
        String time_str = astr_fmt(ui->frame_mem, "%02u:%02u:%02u", time.hours, time.minutes, time.seconds);
        UiBox *clock = ui_label("clock", time_str);
        ui_style_box_vec2(clock, UI_PADDING, vec2(40, 40));
        ui_style_box_from_config(clock, UI_FONT, UI_CONFIG_FONT_MONO);
        ui_style_box_f32(clock, UI_FONT_SIZE, 100.0);
    }
}

static Void build_color_view () {
    ui_box(0, "color_view") {
        ui_style_vec2(UI_PADDING, vec2(16.0, 16));
        ui_style_f32(UI_SPACING, 10.0);
        ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 250, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 350, 1});

        ui_box(0, "graphical_pickers"){
            ui_style_f32(UI_SPACING, 8.0);
            ui_color_sat_val_picker(str("sat_val"), app->hue, &app->sat, &app->val);
            ui_color_hue_picker(str("hue"), &app->hue);
            ui_color_alpha_picker(str("alpha"), &app->alpha);
        }

        ui_box(UI_BOX_INVISIBLE, "spacer") ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 10, 1});

        ui_box(0, "hex_picker") {
            ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
            ui_label("label", str("HEX: "));
            ui_hspacer();
            ui_color_picker(str("picker"), COLOR_PICKER_HEX, &app->hue, &app->sat, &app->val, &app->alpha);
        }

        ui_box(0, "rgba_picker") {
            ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
            ui_label("label", str("RGBA: "));
            ui_hspacer();
            ui_color_picker(str("picker"), COLOR_PICKER_RGBA, &app->hue, &app->sat, &app->val, &app->alpha);
        }

        ui_box(0, "hsva_picker") {
            ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
            ui_label("label", str("HSVA: "));
            ui_hspacer();
            ui_color_picker(str("picker"), COLOR_PICKER_HSVA, &app->hue, &app->sat, &app->val, &app->alpha);
        }
    }
}

static Void build_tile_view () {
    Date date = {};
    ui_date_picker(str("date_picker"), &date);
}

static Void build_misc_view () {
    ui_scroll_box("misc_view") {
        ui_tag("vbox");
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_u32(UI_ANIMATION, UI_MASK_WIDTH);

        ui_style_rule("#misc_view") { ui_style_vec2(UI_PADDING, vec2(80, 16)); }

        ui_box(0, "box2_0") {
            ui_tag("hbox");
            ui_tag("item");

            ui_style_rule("#Foo4") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
            ui_button("Foo4");
            ui_button("Foo5");
        }

        ui_box(0, "box2_1") {
            ui_tag("hbox");
            ui_tag("item");

            ui_button("Foo6");
            ui_button("Foo7");
        }

        ui_box(0, "box2_2") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);

            ui_button("Foo8");
            ui_button("Foo9");
        }

        ui_box(0, "box2_3") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_END);

            ui_button("Foo10");
            ui_button("Foo11");
        }

        ui_scroll_box("box2_4") {
            ui_tag("hbox");
            ui_tag("item");
            for (U64 i = 0; i < cast(U64, 10*app->slider); ++i) {
                String str = astr_fmt(ui->frame_mem, "Foo_%lu", i);
                ui_button_str(str, str);
            }
        }

        ui_box_fmt(0, "slider") {
            ui_tag("hbox");
            ui_tag("item");
            ui_slider("Slider", &app->slider);
        }

        ui_box(0, "box2_5") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_END);

            ui_icon("icon1", 16, get_icon(UI_ICON_TODO));
            ui_icon("icon2", 16, get_icon(UI_ICON_FIRE));
            ui_icon("icon3", 16, get_icon(UI_ICON_EYE));
            ui_icon("icon4", 16, get_icon(UI_ICON_ALARM));
        }

        ui_box(0, "box2_6") {
            ui_tag("hbox");
            ui_tag("item");

            ui_toggle("toggle", &app->toggle);
            ui_checkbox("checkbox", &app->toggle);

            UiBox *popup_button = ui_button("popup_button");
            if (app->popup_shown || popup_button->signals.clicked) {
                ui_tag_box(popup_button, "press");
                ui_popup("popup", &app->popup_shown, false, popup_button) {
                    build_color_view();
                }
            }

            ui_entry(str("entry"), app->buf2, 30, str("Type something..."));
        }

        ui_box(0, "box2_7") {
            ui_tag("hbox");
            ui_tag("item");

            ui_int_picker(str("int_picker"), &app->intval, 0, 10, 3);
            ui_int_picker(str("int_picker2"), &app->intval, 0, 10, 3);
        }

        ui_box(0, "box2_8") {
            ui_tag("hbox");
            ui_tag("item");

            UiBox *img = ui_image("image", &app->image, false, vec4(0,0,0,0), 200);
            UiBox *img_overlay = array_get(&img->children, 0);
            ui_style_box_f32(img_overlay, UI_OUTSET_SHADOW_WIDTH, 2);
            ui_style_box_vec4(img_overlay, UI_OUTSET_SHADOW_COLOR, vec4(0, 0, 0, 1));
        }
    }
}

static Void build_grid_view () {
    ui_scroll_box("second_view") {
        ui_tag("vbox");
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_u32(UI_ANIMATION, UI_MASK_WIDTH);

        ui_style_rule("#second_view") ui_style_vec2(UI_PADDING, vec2(80, 16));

        ui_grid("test_grid") {
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});

            ui_grid_cell(0, 0, 3, 2) { ui_button("1"); }
            ui_grid_cell(3, 0, 5, 2) { ui_button("1"); }
            ui_grid_cell(0, 2, 3, 5) { ui_button("1"); }
            ui_grid_cell(3, 2, 5, 2) { ui_button("1"); }
            ui_grid_cell(3, 4, 3, 2) {
                ui_grid("test_grid") {
                    ui_grid_cell(0, 0, 3, 2);
                    ui_grid_cell(3, 0, 5, 2);
                    ui_grid_cell(0, 2, 3, 5);
                    ui_grid_cell(3, 2, 5, 2);
                    ui_grid_cell(3, 4, 3, 2);
                    ui_grid_cell(6, 4, 2, 2);
                    ui_grid_cell(3, 6, 5, 1);
                }
            }
            ui_grid_cell(6, 4, 2, 2) { ui_button("1"); }
            ui_grid_cell(3, 6, 5, 1) { ui_button("1"); }
        }
    }
}

static Void show_modal () {
    ui_modal("modal", &app->modal_shown) {
        build_color_view();
    }
}

Void app_build () {
    ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_1);

    ui_style_rule(".vbox") {
        ui_style_vec2(UI_PADDING, vec2(8, 8));
        ui_style_f32(UI_SPACING, 8.0);
        ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_1);
        ui_style_vec4(UI_BORDER_COLOR, vec4(0, 0, 0, .4));
        ui_style_f32(UI_EDGE_SOFTNESS, 0);
    }

    ui_style_rule(".hbox") {
        ui_style_vec2(UI_PADDING, vec2(8, 8));
        ui_style_f32(UI_SPACING, 8.0);
        ui_style_u32(UI_AXIS, UI_AXIS_HORIZONTAL);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
        ui_style_vec4(UI_BG_COLOR, vec4(0, 0, 0, .2));
        ui_style_vec4(UI_BORDER_COLOR, vec4(0, 0, 0, .4));
        ui_style_f32(UI_EDGE_SOFTNESS, 0);
    }

    ui_style_rule(".hbox.item") {
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 0});
        ui_style_vec4(UI_BORDER_WIDTHS, vec4(1, 1, 1, 1));
    }

    ui_box(0, "sub_root") {
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});

        ui_box(0, "box1") {
            ui_tag("vbox");
            ui_style_vec4(UI_BORDER_WIDTHS, vec4(1, 0, 0, 0));
            ui_style_size(UI_WIDTH, (UiSize){.tag=UI_SIZE_PCT_PARENT, .value=1./4});
            ui_style_size(UI_HEIGHT, (UiSize){.tag=UI_SIZE_PCT_PARENT, .value=1});

            if (ui_button("Foo1")->signals.clicked && ui->event->key == KEY_MOUSE_LEFT) {
                app->modal_shown = !app->modal_shown;
            }

            if (app->modal_shown) {
                show_modal();
            }

            ui_style_rule("#Foo2") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
            ui_style_rule("#Foo3") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 80, 0}); }
            ui_style_rule("#Foo4") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 80, 0}); }

            UiBox *foo2 = ui_button("Foo2");
            UiBox *foo3 = ui_button("Foo3");
            UiBox *foo4 = ui_button("Foo4");
            UiBox *foo5 = ui_button("Foo5");

            if (foo2->signals.clicked && ui->event->key == KEY_MOUSE_LEFT) app->view = 0;
            if (foo3->signals.clicked && ui->event->key == KEY_MOUSE_LEFT) app->view = 1;
            if (foo4->signals.clicked && ui->event->key == KEY_MOUSE_LEFT) app->view = 2;
            if (foo5->signals.clicked && ui->event->key == KEY_MOUSE_LEFT) app->view = 3;

            switch (app->view) {
            case 0: ui_tag_box(foo2, "press"); break;
            case 1: ui_tag_box(foo3, "press"); break;
            case 2: ui_tag_box(foo4, "press"); break;
            case 3: ui_tag_box(foo5, "press"); break;
            }
        }

        switch (app->view) {
        case 0: build_misc_view(); break;
        case 1: build_grid_view(); break;
        case 2: build_text_view(); break;
        case 3: build_tile_view(); break;
        }
    }
}

Void app_init () {
    app = mem_new(ui->perm_mem, App);
    app->view = 3;
    app->image = dr_image("data/images/screenshot.png", false);
    app->slider = .5;
    app->buf1 = buf_new_from_file(ui->perm_mem, str("/home/zagor/Documents/test.txt"));
    app->buf2 = buf_new(ui->perm_mem, str("asdf"));
    app->hue = .3;
}
