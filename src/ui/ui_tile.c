#include "ui/ui_tile.h"
#include "ui/ui_widgets.h"
#include "window/window.h"

static Void build_node (UiTileNode *node, ArrayUiTileLeaf *out_leafs) {
    F32 b = ui_config_get_vec4(UI_CONFIG_BORDER_1_WIDTH).x;
    U32 splitter_width = 8;

    if (node->split != UI_TILE_SPLIT_NONE) {
        ui_style_u32(UI_AXIS, node->split == UI_TILE_SPLIT_HORI ? UI_AXIS_HORIZONTAL : UI_AXIS_VERTICAL);

        UiBox *first = ui_box(UI_BOX_CLICK_THROUGH, "first") {
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
            ui_style_f32(UI_EDGE_SOFTNESS, 0);

            if (node->split == UI_TILE_SPLIT_HORI) {
                ui_style_vec4(UI_BORDER_WIDTHS, vec4(b, 0, 0, 0));
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, node->ratio, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
            } else {
                ui_style_vec4(UI_BORDER_WIDTHS, vec4(0, 0, 0, b));
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, node->ratio, 1});
            }

            build_node(node->child[0], out_leafs);
        }

        UiBox *splitter = ui_box(UI_BOX_REACTIVE, "splitter") {
            if (node->split == UI_TILE_SPLIT_HORI) {
                ui_style_f32(UI_FLOAT_X, first->rect.w - splitter_width);
                ui_style_f32(UI_FLOAT_Y, 0);
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, splitter_width, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
                if (splitter->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE) {
                    node->ratio = (ui->mouse.x - splitter->parent->rect.x) / splitter->parent->rect.w;
                }
                if (splitter->signals.hovered || splitter->signals.pressed) ui->requested_cursor = MOUSE_CURSOR_EW_RESIZE;
            } else {
                ui_style_f32(UI_FLOAT_X, 0);
                ui_style_f32(UI_FLOAT_Y, first->rect.h - splitter_width);
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, splitter_width, 1});
                if (splitter->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE) {
                    F32 parent_h = splitter->parent->rect.h;
                    if (parent_h > 0) node->ratio += ui->mouse_dt.y / parent_h;
                }
                if (splitter->signals.hovered || splitter->signals.pressed) ui->requested_cursor = MOUSE_CURSOR_NS_RESIZE;
            }

            node->ratio = clamp(node->ratio, 0.1f, 0.9f);
        }

        ui_box(UI_BOX_CLICK_THROUGH, "second") {
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);

            if (node->split == UI_TILE_SPLIT_HORI) {
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1.f - node->ratio, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
            } else {
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1.f - node->ratio, 1});
            }

            build_node(node->child[1], out_leafs);
        }
    } else {
        tmem_new(tm)
        UiBox *box;

        ui_box(0, "leaf") {
            ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);

            ui_box(0, "tabs_panel") {
                ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
                ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
                ui_style_vec4(UI_BORDER_WIDTHS, vec4(0, 0, 0, b));
                ui_style_f32(UI_EDGE_SOFTNESS, 0);

                Vec2 padding = ui_config_get_vec2(UI_CONFIG_PADDING_1);

                // @todo We wouldn't need this if the padding style were a vec4 instead of a vec2.
                ui_box(0, "top_padding") ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, padding.x, 1});

                ui_box (0, "tabs") {
                    ui_style_vec2(UI_PADDING, vec2(padding.x, 0));
                    ui_style_from_config(UI_SPACING, UI_CONFIG_SPACING_1);

                    array_iter (id, &node->tab_ids) {
                        ui_box_fmt (0, "tab%lu", ARRAY_IDX) {
                            ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
                            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
                            F32 r = ui_config_get_vec4(UI_CONFIG_RADIUS_1).x;
                            ui_style_vec4(UI_RADIUS, vec4(r, r, 0, 0));
                            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);

                            if (ARRAY_IDX == node->active_tab_idx) {
                                ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_SELECTION);
                                ui_style_from_config(UI_TEXT_COLOR, UI_CONFIG_TEXT_SELECTION);
                            } else {
                                ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
                            }

                            ui_box (0, "label_box") {
                                ui_style_vec2(UI_PADDING, vec2(6, 4));
                                ui_label(0, "label", astr_fmt(tm, "%lu", id));
                            }

                            ui_button(str("close_button")) {
                                ui_style_vec2(UI_PADDING, vec2(2, 2));
                                ui_style_vec4(UI_BG_COLOR, vec4(0, 0, 0, 0));
                                ui_style_vec4(UI_BG_COLOR2, vec4(-1, 0, 0, 0));
                                ui_style_vec4(UI_BG_COLOR2, vec4(-1, 0, 0, 0));
                                ui_style_f32(UI_OUTSET_SHADOW_WIDTH, 0);
                                ui_icon(UI_BOX_CLICK_THROUGH, "icon", 16, UI_ICON_CLOSE);
                            }
                        }
                    }
                }
            }

            box = ui_box(0, "content");
        }

        U64 active_tab_id = array_get(&node->tab_ids, node->active_tab_idx);
        array_push_lit(out_leafs, .node=node, .active_tab_id=active_tab_id, .box=box);
    }
}

UiBox *ui_tile (String id, UiTileNode *tree, ArrayUiTileLeaf *out_leafs) {
    UiBox *container = ui_box_str(0, id) {
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
        build_node(tree, out_leafs);
    }

    return container;
}
