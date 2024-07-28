#pragma once

#include "config.h"

#if CONFIG_FONT_TTF

#include <memory>
#include <optional>
#include <unordered_map>

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

public:
    TTFCache() {}

    ~TTFCache() {
        showStats();
        clear();
    }

    auto getGlyph(Font &font, GlyphCode glyphCode, int16_t charSize)
        -> std::optional<const Glyph *>;

    void clear();
    void showStats();

    void showBitmap(const Bitmap &bitmap, bool inverted, PixelResolution pixelResolution) const;

    void showGlyph(const Glyph &glyph, bool displayBitmap, PixelResolution pixelResolution) const;
};

#endif