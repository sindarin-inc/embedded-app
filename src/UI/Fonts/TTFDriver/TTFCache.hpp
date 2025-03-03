#pragma once

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
    SpiramMap<uint32_t, std::shared_ptr<Glyph>> glyphCache_;
    uint32_t hitCount_ = 0;
    uint32_t missCount_ = 0;

    auto doGetGlyph(Font &font, GlyphCode glyphCode, uint32_t key) -> std::optional<const Glyph *>;

public:
    TTFCache() = default;

    ~TTFCache() {
        showStats();
        clear();
    }

    inline auto getGlyph(Font &font, GlyphCode glyphCode, int16_t charSize)
        -> std::optional<const Glyph *> {

        auto key = static_cast<uint32_t>(static_cast<uint32_t>(charSize << 16) | glyphCode);
        auto it = glyphCache_.find(key);
        if (it != glyphCache_.end()) {
            // log_d("hit(%" PRIu16 ") -> %p!", glyphCode, (void *)it->second);
            hitCount_++;
            return it->second.get();
        }

        return doGetGlyph(font, glyphCode, key);
    }

    void clear();
    void showStats() const;

    void showBitmap(const Bitmap &bitmap, bool inverted, PixelResolution pixelResolution) const;

    void showGlyph(const Glyph &glyph, bool displayBitmap, PixelResolution pixelResolution) const;
};

#endif