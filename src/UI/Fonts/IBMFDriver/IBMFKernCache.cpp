#include "config.h"

#if CONFIG_FONT_IBMF

#include "IBMFFace.hpp"
#include "IBMFKernCache.hpp"

auto IBMFKernCache::getOpticalKern(IBMFFace &face, GlyphCode glyphCode1, GlyphCode glyphCode2,
                                   int16_t faceIndex) -> FIX16 {

    auto key = (uint32_t)((glyphCode1 << 16) | glyphCode2);
    auto it = kernCache.find(key);
    if (it != kernCache.end()) {
        hitCount++;
        return it->second;
    }

    FIX16 kern;
    kern = face.opticalKern(glyphCode1, glyphCode2);
    kernCache.insert({key, kern});
    missCount++;
    return kern;
}

void IBMFKernCache::clear() {
    kernCache.clear();
    hitCount = missCount = 0;
    log_i("Kerns' cache cleared.");
}

void IBMFKernCache::showStats() {
    log_i("Kerns' cache statistics: hits: %" PRIu32 ", misses: %" PRIu32 ".", hitCount, missCount);
}

#endif