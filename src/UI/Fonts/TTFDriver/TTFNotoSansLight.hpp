#pragma once

#if CONFIG_FONT_TTF

#include "../TTFFonts/NotoSans-Light.h"
#include "../TTFFonts/SolPrivate-Light.h"
#include "TTFFontData.hpp"

class TTFNotoSansLight : public FontData {
public:
    ~TTFNotoSansLight() override = default;
    TTFNotoSansLight() = default;
    ;

    auto ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const
        -> bool override;

    [[nodiscard]] auto getData() const -> MemoryPtr override {
        return const_cast<MemoryPtr>(notoSansLight.data_.data());
    }
    [[nodiscard]] auto getDataSize() const -> int override { return notoSansLight.data_.size(); }

    [[nodiscard]] auto getPrivateData() const -> MemoryPtr override {
        return const_cast<MemoryPtr>(solPrivateLight.data_.data());
    }
    [[nodiscard]] auto getPrivateDataSize() const -> int override {
        return solPrivateLight.data_.size();
    }
};

#endif