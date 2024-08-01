#pragma once

#include "config.h"

#if CONFIG_FONT_TTF

#include <memory>
#include <optional>

#include "Misc/SpiramAllocator.hpp"
#include "TTFDefs.hpp"
#include "UI/Fonts/FontDefs.hpp"

using namespace font_defs;

class Font;

class TTFCache {
private:
    SpiramMap<uint32_t, std::shared_ptr<Glyph>> glyphCache;
    uint32_t hitCount = 0;
    uint32_t missCount = 0;

    auto doGetGlyph(Font &font, GlyphCode glyphCode, uint32_t key) -> std::optional<const Glyph *>;

public:
    TTFCache() {}

    ~TTFCache() {
        showStats();
        clear();
    }

    inline auto getGlyph(Font &font, GlyphCode glyphCode,
                         int16_t charSize) -> std::optional<const Glyph *> {

        auto key = (uint32_t)((glyphCode << 16) | charSize);
        auto it = glyphCache.find(key);
        if (it != glyphCache.end()) {
            // log_d("hit(%" PRIu16 ") -> %p!", glyphCode, (void *)it->second);
            hitCount++;
            return it->second.get();
        }

        return doGetGlyph(font, glyphCode, key);
    }

    void clear();
    void showStats();

    void showBitmap(const Bitmap &bitmap, bool inverted, PixelResolution pixelResolution) const;

    void showGlyph(const Glyph &glyph, bool displayBitmap, PixelResolution pixelResolution) const;
};

#endif