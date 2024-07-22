#pragma once

#include "config.h"

#if CONFIG_FONT_TTF

#include "../TTFFonts/NotoSans-Light.h"
#include "../TTFFonts/SolPrivate-Light.h"
#include "TTFFontData.hpp"

class TTFNotoSansLight : public FontData {
public:
    TTFNotoSansLight() : FontData(){};

    auto ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const
        -> bool override;

    auto getData() const -> MemoryPtr override { return (MemoryPtr)(notoSansLight.data_.data()); }
    auto getDataSize() const -> int override { return notoSansLight.data_.size(); }

    auto getPrivateData() const -> MemoryPtr override {
        return (MemoryPtr)(solPrivateLight.data_.data());
    }
    auto getPrivateDataSize() const -> int override { return solPrivateLight.data_.size(); }
};

#endif