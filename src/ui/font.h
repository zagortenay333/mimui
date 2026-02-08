#include <hb.h>
#include <hb-ft.h>
#include "base/core.h"
#include "base/map.h"

istruct (GlyphInfo) {
    U32 x;
    U32 y;
    U32 x_advance;
    U32 y_advance;
    U32 codepoint;
    U32 glyph_index;
};

istruct (GlyphSlot) {
    U16 x;
    U16 y;
    U32 width;
    U32 height;
    I32 bearing_x;
    I32 bearing_y;
    I32 advance;
    FT_Pixel_Mode pixel_mode;
    U32 glyph_index;
    GlyphSlot *lru_next;
    GlyphSlot *lru_prev;
};

istruct (FontCache);

istruct (Font) {
    FontCache *cache;

    String filepath;
    FT_Face ft_face;
    hb_face_t *hb_face;
    hb_font_t *hb_font;

    Bool is_mono;
    U32 size;
    U32 height;
    U32 width;
    U32 ascent;
    U32 descent;

    GlyphSlot lru;
    Array(GlyphSlot*) free_slots;
    Map(U32, GlyphSlot*) slot_map;

    U32 atlas_texture;
    U16 atlas_slot_size;
};

typedef Void (*VertexFlushFn)();

istruct (FontCache) {
    Mem *mem;
    Array(Font*) fonts;
    FT_Library ft_lib;
    VertexFlushFn vertex_flush_fn;
    U16 atlas_size;
};

array_typedef(GlyphInfo, GlyphInfo);

FontCache     *font_cache_new       (Mem *, VertexFlushFn, U16 atlas_size);
Void           font_cache_destroy   (FontCache *);
Font          *font_get             (FontCache *, String filepath, U32 size, Bool is_mono);
GlyphSlot     *font_get_glyph_slot  (Font *, GlyphInfo *);
SliceGlyphInfo font_get_glyph_infos (Font *, Mem *, String);
