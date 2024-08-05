#include "config.h"

#if CONFIG_FONT_IBMF

#include "IBMFFace.hpp"
#include "IBMFKernCache.hpp"

auto IBMFKernCache::doGetOpticalKern(IBMFFace &face, GlyphCode glyphCode1,
                                     GlyphCode glyphCode2) -> FIX16 {

    FIX16 kern;
    kern = face.opticalKern(glyphCode1, glyphCode2);
    kernCache.insert({(uint32_t)((glyphCode1 << 16) | glyphCode2), kern});
    missCount++;
    return kern;
}

void IBMFKernCache::clear() {
    kernCache.clear();
    hitCount = missCount = 0;
    log_i("Kerns' cache cleared.");
}

void IBMFKernCache::showStats() const {
    log_i("Kerns' cache statistics: hits: %" PRIu32 ", misses: %" PRIu32 ".", hitCount, missCount);
}

#endif