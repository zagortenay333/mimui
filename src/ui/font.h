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

typedef U64 GlyphId;

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

    // Private:
    GlyphId id;
    GlyphSlot *lru_next;
    GlyphSlot *lru_prev;
    GlyphSlot *map_next;
};

istruct (FontCache);

istruct (Font) {
    FontCache *cache;

    String filepath;
    FT_Face ft_face;
    hb_face_t *hb_face;
    hb_font_t *hb_font;

    U32 size;
    U32 height;
    U32 width;
    U32 ascent;
    U32 descent;

    GlyphSlot **map;
    GlyphSlot *slots;
    GlyphSlot sentinel;

    U32 atlas_texture;
    U16 atlas_slot_size;
};

typedef Void (*GlyphEvictionFn)();

istruct (FontCache) {
    Mem *mem;
    Array(Font*) fonts;
    FT_Library ft_lib;
    GlyphEvictionFn evict_fn;
    U16 atlas_size;
};

array_typedef(GlyphInfo, GlyphInfo);

FontCache     *font_cache_new       (Mem *, GlyphEvictionFn, U16 atlas_size);
Void           font_cache_destroy   (FontCache *);
Font          *font_get             (FontCache *, String filepath, U32 size);
GlyphSlot     *font_get_glyph_slot  (Font *, GlyphInfo *);
SliceGlyphInfo font_get_glyph_infos (Font *, Mem *, String);
