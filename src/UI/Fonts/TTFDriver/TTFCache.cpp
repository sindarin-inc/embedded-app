#include "config.h"

#if CONFIG_FONT_TTF

#include "TTFCache.hpp"
#include "TTFFont.hpp"

auto TTFCache::getGlyph(Font &font, font_defs::GlyphCode glyphCode,
                        int16_t charSize) -> std::optional<font_defs::Glyph *> {

    // log_d("glyph cache request for glyphCode %" PRIu16 " and size %" PRIi16 ".", glyphCode,
    //       charSize);

    auto key = (uint32_t)((glyphCode << 16) | charSize);
    auto it = glyphCache.find(key);
    if (it != glyphCache.end()) {
        // log_d("hit(%" PRIu16 ") -> %p!", glyphCode, (void *)it->second);
        hitCount++;
        return it->second.get();
    }

    // auto glyph = new font_defs::Glyph();
    auto allocator = SpiramAllocator<font_defs::Glyph>();
    std::shared_ptr<font_defs::Glyph> glyph = allocator.allocateShared();

    if (glyph == nullptr) {
        clear();
        glyph = allocator.allocateShared();

        if (glyph == nullptr) {
            log_e("Unable to allocate SPIRAM memory for a glyph.");
        }
    }

    if (font.getGlyph(glyphCode, *glyph.get(), true, true, Pos(0, 0), false, false)) {
        glyphCache.insert({key, glyph});
        // log_d("miss (%" PRIu16 ") -> %p!", glyphCode, glyph);
        missCount++;
        return glyph.get();
    } else {
        clear();
        if (font.getGlyph(glyphCode, *glyph.get(), true, true, Pos(0, 0), false, false)) {
            glyphCache.insert({key, glyph});
            // log_d("miss (%" PRIu16 ") -> %p!", glyphCode, glyph);
            missCount++;
            return glyph.get();
        } else {
            log_e("Unable to allocate SPIRAM memory for a glyph.");
        }
    }

    return std::nullopt;
}

void TTFCache::clear() {
    for (auto &entry : glyphCache) {
        free(entry.second->bitmap.pixels);
    }
    glyphCache.clear();
    hitCount = missCount = 0;
    log_w("Glyphs' cache cleared.");
}

void TTFCache::showStats() {
    log_i("Glyphs' cache statistics: hits: %" PRIu32 ", misses: %" PRIu32 ".", hitCount, missCount)
}

#endif