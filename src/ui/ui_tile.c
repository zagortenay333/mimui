#include "ui/ui_tile.h"
#include "ui/ui_widgets.h"

static Void build_node (UiTileNode *node, ArrayUiTileLeaf *out_leafs) {
    F32 b = ui_config_get_vec4(UI_CONFIG_BORDER_1_WIDTH).x;

    if (node->split != UI_TILE_SPLIT_NONE) {
        ui_style_u32(UI_AXIS, node->split == UI_TILE_SPLIT_HORI ? UI_AXIS_HORIZONTAL : UI_AXIS_VERTICAL);

        ui_box(0, "left") {
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

        ui_box(0, "right") {
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
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 1});
                ui_style_from_config(UI_PADDING, UI_CONFIG_PADDING_1);
                ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
                ui_style_vec4(UI_BORDER_WIDTHS, vec4(0, 0, 0, b));
                ui_style_f32(UI_EDGE_SOFTNESS, 0);

                array_iter (id, &node->tab_ids) {
                    ui_box_fmt (0, "tab%lu", ARRAY_IDX) {
                        ui_label(0, "label", astr_fmt(tm, "%lu", id));
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
        build_node(tree, out_leafs);
    }

    return container;
}
