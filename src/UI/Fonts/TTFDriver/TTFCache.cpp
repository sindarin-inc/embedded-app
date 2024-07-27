#include "config.h"

#if CONFIG_FONT_TTF

#include "TTFCache.hpp"
#include "TTFFont.hpp"

auto TTFCache::getGlyph(Font &font, font_defs::GlyphCode glyphCode,
                        int16_t charSize) -> std::optional<const font_defs::Glyph *> {

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

    if (font.getGlyphForCache(glyphCode, *glyph.get())) {
        glyphCache.insert({key, glyph});
        // log_d("miss (%" PRIu16 ") -> %p!", glyphCode, glyph);
        missCount++;
        return glyph.get();
    } else {
        clear();
        if (font.getGlyphForCache(glyphCode, *glyph.get())) {
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
    showStats();
    for (auto &entry : glyphCache) {
        free(entry.second->bitmap.pixels);
    }
    glyphCache.clear();
    hitCount = missCount = 0;
    log_w("Glyphs' cache cleared.");
}

void TTFCache::showStats() {
    log_i("Glyphs' cache statistics: hits: %" PRIu32 ", misses: %" PRIu32 ".", hitCount, missCount);
}

void TTFCache::showBitmap(const Bitmap &bitmap, bool inverted,
                          PixelResolution pixelResolution) const {
    uint32_t row, col;
    MemoryPtr rowPtr;

    uint32_t maxWidth = 50;
    if (bitmap.dim.width < maxWidth) {
        maxWidth = bitmap.dim.width;
    }

    std::cout << "   +";
    for (col = 0; col < maxWidth; col++) {
        std::cout << '-';
    }
    std::cout << '+' << std::endl << std::flush;

    if (pixelResolution == PixelResolution::ONE_BIT) {
        uint32_t rowSize = bitmap.pitch;
        for (row = 0, rowPtr = bitmap.pixels; row < bitmap.dim.height; row++, rowPtr += rowSize) {
            std::cout << "   |";
            for (col = 0; col < maxWidth; col++) {
                if (inverted) {
                    std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? ' ' : 'X');
                } else {
                    std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
                }
            }
            std::cout << '|';
            std::cout << std::endl << std::flush;
        }
    } else {
        uint32_t rowSize = bitmap.pitch;
        for (row = 0, rowPtr = bitmap.pixels; row < bitmap.dim.height; row++, rowPtr += rowSize) {
            std::cout << "   |";
            for (col = 0; col < maxWidth; col++) {
                if (inverted) {
                } else {
                }
            }
            std::cout << '|';
            std::cout << std::endl << std::flush;
        }
    }

    std::cout << "   +";
    for (col = 0; col < maxWidth; col++) {
        std::cout << '-';
    }
    std::cout << '+' << std::endl << std::endl << std::flush;
}

void TTFCache::showGlyph(const Glyph &glyph, bool displayBitmap,
                         PixelResolution pixelResolution) const {
    std::cout << "Glyph metrics: xoff:" << glyph.metrics.xoff << " yoff:" << glyph.metrics.yoff
              << " descent:" << glyph.metrics.descent << " lineHeight:¨" << glyph.metrics.lineHeight
              << " advance:" << (glyph.metrics.advance / 64) << std::endl;
    std::cout << "Bitmap dimensions:[" << glyph.bitmap.dim.width << "x" << glyph.bitmap.dim.height
              << "] pitch:" << glyph.bitmap.pitch << std::endl;

    if (displayBitmap) {
        showBitmap(glyph.bitmap, false, pixelResolution);
    }
}
#endif