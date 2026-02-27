#include "app/app.h"
#include "ui/ui.h"
#include "ui/ui_widgets.h"
#include "ui/ui_text_editor.h"
#include "ui/ui_tile.h"
#include "buffer/buffer.h"
#include "window/window.h"

istruct (App) {
    Buf *buf1;
    Buf *buf2;

    Bool modal_shown;
    Bool popup_shown;
    Bool calendar_popup_shown;

    UiTileNode *tile_root;

    Date date;
    Time time;

    struct {
        Key key;
        KeyMod mod;
    } shortcut;

    struct {
        U64 idx;
        SliceString slice;
    } selections;

    UiViewStore *view_store;
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

Void view_clock_init (UiViewInstance *instance) {
}

Void view_clock_free (UiViewInstance *instance) {
}

UiIcon view_clock_get_icon (UiViewInstance *instance, Bool visible) {
    return UI_ICON_STOPWATCH;
}

String view_clock_get_title (UiViewInstance *instance, Bool visible) {
    return str("Clock");
}

Void view_clock_build (UiViewInstance *instance, Bool visible) {
    if (! visible) return;
    ui_box(UI_BOX_CLICK_THROUGH, "clock_box") {
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CHILDREN_SUM, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 1, 0});
        ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);

        Time time = os_get_wall_time();
        String time_str = astr_fmt(ui->frame_mem, "%02u:%02u:%02u", time.hours, time.minutes, time.seconds);
        UiBox *clock = ui_label(0, "clock", time_str);
        ui_style_box_vec2(clock, UI_PADDING, vec2(40, 40));
        ui_style_box_from_config(clock, UI_FONT, UI_CONFIG_FONT_MONO);
        ui_style_box_f32(clock, UI_FONT_SIZE, 100.0);
    }
}

Void view_text_init (UiViewInstance *instance) {
}

Void view_text_free (UiViewInstance *instance) {
}

UiIcon view_text_get_icon (UiViewInstance *instance, Bool visible) {
    return UI_ICON_FIRE;
}

String view_text_get_title (UiViewInstance *instance, Bool visible) {
    return str("Text");
}

Void view_text_build (UiViewInstance *instance, Bool visible) {
    if (! visible) return;
    UiBox *box = ui_ted(str("text_box"), app->buf1, false, LINE_WRAP_NONE);
    ui_style_box_size(box, UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_box_size(box, UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_box_vec2(box, UI_PADDING, (Vec2){8, 8});
}

Void view_grid_init (UiViewInstance *instance) {
}

Void view_grid_free (UiViewInstance *instance) {
}

UiIcon view_grid_get_icon (UiViewInstance *instance, Bool visible) {
    return UI_ICON_HEATMAP;
}

String view_grid_get_title (UiViewInstance *instance, Bool visible) {
    return str("Grid");
}

Void view_grid_build (UiViewInstance *instance, Bool visible) {
    if (! visible) return;

    ui_scroll_box(str("second_view"), true) {
        ui_tag("vbox");
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_u32(UI_ANIMATION, UI_MASK_WIDTH);

        ui_grid(str("test_grid")) {
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});

            ui_style_rule(".grid_cell") {
                ui_style_from_config(UI_PADDING, UI_CONFIG_PADDING_1);
                ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
                ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
                ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
                ui_style_f32(UI_EDGE_SOFTNESS, 0);
            }

            ui_grid_cell(0, 0, 3, 2) { ui_button_label("1"); }
            ui_grid_cell(3, 0, 5, 2) { ui_button_label("1"); }
            ui_grid_cell(0, 2, 3, 5) { ui_button_label("1"); }
            ui_grid_cell(3, 2, 5, 2) { ui_button_label("1"); }
            ui_grid_cell(3, 4, 3, 2) {
                ui_grid(str("test_grid")) {
                    ui_grid_cell(0, 0, 3, 2);
                    ui_grid_cell(3, 0, 5, 2);
                    ui_grid_cell(0, 2, 3, 5);
                    ui_grid_cell(3, 2, 5, 2);
                    ui_grid_cell(3, 4, 3, 2);
                    ui_grid_cell(6, 4, 2, 2);
                    ui_grid_cell(3, 6, 5, 1);
                }
            }
            ui_grid_cell(6, 4, 2, 2) { ui_button_label("1"); }
            ui_grid_cell(3, 6, 5, 1) { ui_button_label("1"); }
        }
    }
}

Void view_misc_init (UiViewInstance *instance) {
}

Void view_misc_free (UiViewInstance *instance) {
}

UiIcon view_misc_get_icon (UiViewInstance *instance, Bool visible) {
    return UI_ICON_HOME;
}

String view_misc_get_title (UiViewInstance *instance, Bool visible) {
    return str("Misc");
}

Void view_misc_build (UiViewInstance *instance, Bool visible) {
    if (! visible) return;

    ui_scroll_box(str("misc_view"), true) {
        ui_tag("vbox");
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_u32(UI_ANIMATION, UI_MASK_WIDTH);

        ui_style_rule("#misc_view") { ui_style_vec2(UI_PADDING, vec2(80, 16)); }

        ui_box(0, "box2_0") {
            ui_tag("hbox");
            ui_tag("item");

            ui_style_rule("#Foo4") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
            ui_button_label("Foo4");
            ui_button_label("Foo5");
        }

        ui_box(0, "box2_1") {
            ui_tag("hbox");
            ui_tag("item");

            ui_button_label("Foo6");
            ui_button_label("Foo7");
        }

        ui_box(0, "box2_2") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);

            ui_button_group(str("buttons")) {
                ui_button_label("Foo8");
                ui_button_label("Foo9");
                ui_button_label("Foo10");
            }
        }

        ui_box(0, "box2_3") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_END);

            ui_button_label("Foo10");
            ui_button_label("Foo11");
        }

        ui_scroll_box(str("box2_4"), true) {
            ui_tag("hbox");
            ui_tag("item");
            for (U64 i = 0; i < cast(U64, 10*app->slider); ++i) {
                String str = astr_fmt(ui->frame_mem, "Foo_%lu", i);
                ui_button_label_str(str, str);
            }
        }

        ui_box(0, "slider") {
            ui_tag("hbox");
            ui_tag("item");
            ui_slider("Slider", &app->slider);
        }

        ui_box(0, "box2_5") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_END);

            ui_icon(0, "icon1", 16, UI_ICON_TODO);
            ui_icon(0, "icon2", 16, UI_ICON_FIRE);
            ui_icon(0, "icon3", 16, UI_ICON_EYE);
            ui_icon(0, "icon4", 16, UI_ICON_ALARM);
        }

        ui_box(0, "box2_6") {
            ui_tag("hbox");
            ui_tag("item");

            ui_toggle("toggle", &app->toggle);
            ui_checkbox("checkbox", &app->toggle);
            ui_color_picker_button(str("color_picker"), &app->hue, &app->sat, &app->val, &app->alpha);
            ui_file_picker_entry(str("file_picker"), app->buf2, true, false);
        }

        ui_box(0, "box2_7") {
            ui_tag("hbox");
            ui_tag("item");

            ui_int_picker(str("int_picker"), &app->intval, 0, 14, 3);
            ui_int_picker(str("int_picker2"), &app->intval, 0, 14, 3);

            UiBox *popup_button = ui_button_label("calendar");
            if (app->calendar_popup_shown || popup_button->signals.clicked) {
                ui_tag_box(popup_button, "press");
                ui_popup(str("popup"), &app->calendar_popup_shown, false, popup_button) {
                    ui_date_picker(str("date_picker"), &app->date);
                }
            }

            ui_shortcut_picker(str("shortcut_picker"), &app->shortcut.key, &app->shortcut.mod);
            ui_dropdown(str("dropdown"), &app->selections.idx, app->selections.slice);
            ui_time_picker(str("time_picker"), &app->time, TIME_PICKER_ALARM);
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
        ui_tile(str("tiles"), ui->perm_mem, &app->tile_root, app->view_store);
    }
}

Void app_init () {
    app = mem_new(ui->perm_mem, App);

    app->view = 3;
    app->image = dr_image("data/images/screenshot.png", false);
    app->slider = .5;
    app->buf1 = buf_new_from_file(ui->perm_mem, str("/home/zagor/Documents/test.txt"));
    app->buf2 = buf_new(ui->perm_mem, str(""));
    app->hue = .3;
    app->val = .3;
    app->sat = .3;
    app->alpha = 1;
    app->date = os_get_date();

    ArrayString a;
    array_init(&a, ui->perm_mem);
    array_push_n(&a, str("Hello"), str("There"), str("Sailor"), str("How"), str("Are"));
    app->selections.slice = a.as_slice;

    app->view_store = ui_view_store_new(ui->perm_mem);

    ui_view_type_add(app->view_store, (UiViewType){
        .static_name = str("misc"),
        .init = view_misc_init,
        .free = view_misc_free,
        .build = view_misc_build,
        .get_icon = view_misc_get_icon,
        .get_title = view_misc_get_title,
    });

    ui_view_type_add(app->view_store, (UiViewType){
        .static_name = str("grid"),
        .init = view_grid_init,
        .free = view_grid_free,
        .build = view_grid_build,
        .get_icon = view_grid_get_icon,
        .get_title = view_grid_get_title,
    });

    ui_view_type_add(app->view_store, (UiViewType){
        .static_name = str("text"),
        .init = view_text_init,
        .free = view_text_free,
        .build = view_text_build,
        .get_icon = view_text_get_icon,
        .get_title = view_text_get_title,
    });

    ui_view_type_add(app->view_store, (UiViewType){
        .static_name = str("clock"),
        .init = view_clock_init,
        .free = view_clock_free,
        .build = view_clock_build,
        .get_icon = view_clock_get_icon,
        .get_title = view_clock_get_title,
    });

    UiViewInstance *view_grid  = ui_view_instance_new(app->view_store, str("grid"));
    UiViewInstance *view_clock = ui_view_instance_new(app->view_store, str("clock"));
    UiViewInstance *view_misc  = ui_view_instance_new(app->view_store, str("misc"));
    UiViewInstance *view_text  = ui_view_instance_new(app->view_store, str("text"));

    { // Build initial tile tree:
        // Root: Horizontal split (Left sidebar 25%, Right main area 75%)
        app->tile_root = mem_new(ui->perm_mem, UiTileNode);
        app->tile_root->split = UI_TILE_SPLIT_HORI;
        app->tile_root->ratio = 0.25f;

        // Left Sidebar (Leaf node)
        UiTileNode *left_panel = mem_new(ui->perm_mem, UiTileNode);
        left_panel->split = UI_TILE_SPLIT_NONE;
        left_panel->parent = app->tile_root;
        array_init(&left_panel->tab_ids, ui->perm_mem);
        array_push(&left_panel->tab_ids, view_grid);
        array_push(&left_panel->tab_ids, view_clock);

        // Right Main Area: Vertical split (Top Viewport 70%, Bottom Console 30%)
        UiTileNode *right_split = mem_new(ui->perm_mem, UiTileNode);
        right_split->split = UI_TILE_SPLIT_VERT;
        right_split->ratio = 0.7f;
        right_split->parent = app->tile_root;

        // Top Viewport (Leaf node)
        UiTileNode *main_panel = mem_new(ui->perm_mem, UiTileNode);
        main_panel->split = UI_TILE_SPLIT_NONE;
        array_init(&main_panel->tab_ids, ui->perm_mem);
        array_push(&main_panel->tab_ids, view_text);
        main_panel->active_tab_idx = 0;
        main_panel->parent = right_split;

        // Bottom Console (Leaf node)
        UiTileNode *bottom_panel = mem_new(ui->perm_mem, UiTileNode);
        bottom_panel->split = UI_TILE_SPLIT_NONE;
        array_init(&bottom_panel->tab_ids, ui->perm_mem);
        array_push(&bottom_panel->tab_ids, view_misc);
        bottom_panel->active_tab_idx = 0;
        bottom_panel->parent = right_split;

        // Link the tree together
        app->tile_root->child[0] = left_panel;
        app->tile_root->child[1] = right_split;

        right_split->child[0] = main_panel;
        right_split->child[1] = bottom_panel;
    }
}
