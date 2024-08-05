#pragma once

#include "config.h"

#if CONFIG_FONT_IBMF

#include <memory>
#include <optional>
#include <unordered_map>

#include "IBMFDefs.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "UI/Fonts/FontDefs.hpp"

using namespace font_defs;

class IBMFFace;

class IBMFKernCache {
private:
    SpiramMap<uint32_t, FIX16> kernCache;
    uint32_t hitCount = 0;
    uint32_t missCount = 0;

public:
    IBMFKernCache() {}

    ~IBMFKernCache() {
        showStats();
        clear();
    }

    auto doGetOpticalKern(IBMFFace &face, GlyphCode glyphCode1, GlyphCode glyphCode2) -> FIX16;

    inline auto getOpticalKern(IBMFFace &face, GlyphCode glyphCode1,
                               GlyphCode glyphCode2) -> FIX16 {

        auto key = (uint32_t)((glyphCode1 << 16) | glyphCode2);
        auto it = kernCache.find(key);
        if (it != kernCache.end()) {
            hitCount++;
            return it->second;
        }

        return doGetOpticalKern(face, glyphCode1, glyphCode2);
    }

    void clear();
    void showStats() const;
};

#endif