#pragma once

#include "base/core.h"
#include "base/array.h"
#include "ui/ui.h"

ienum (UiTileSplit, U8) {
    UI_TILE_SPLIT_NONE,
    UI_TILE_SPLIT_HORI,
    UI_TILE_SPLIT_VERT,
};

istruct (UiTileNode) {
    UiTileSplit split;

    F32 ratio;
    UiTileNode *parent;
    UiTileNode *child[2];

    ArrayU64 tab_ids;
    U64 active_tab_idx;
};

istruct (UiTileTree) {
    Mem *mem;
    UiTileNode *root;
};

istruct (UiTileLeaf) {
    UiTileNode *node;
    U64 active_tab_id;
    UiBox *box;
};

array_typedef(UiTileLeaf, UiTileLeaf);

UiBox *ui_tile (String id, UiTileTree *, ArrayUiTileLeaf *out_leafs);
