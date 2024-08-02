#pragma once

#include "config.h"

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
        return const_cast<MemoryPtr>(NOTO_SANS_LIGHT.data.data());
    }
    [[nodiscard]] auto getDataSize() const -> int override { return NOTO_SANS_LIGHT.data.size(); }

    [[nodiscard]] auto getPrivateData() const -> MemoryPtr override {
        return const_cast<MemoryPtr>(SOL_PRIVATE_LIGHT.data.data());
    }
    [[nodiscard]] auto getPrivateDataSize() const -> int override {
        return SOL_PRIVATE_LIGHT.data.size();
    }
};

#endif