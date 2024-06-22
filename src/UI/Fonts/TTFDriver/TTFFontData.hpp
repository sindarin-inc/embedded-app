#pragma once

#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../TTFFonts/NotoSans-Light.h"
#include "TTFDefs.hpp"
#include "config.h"

using namespace ttf_defs;
using namespace font_defs;

#include <ft2build.h>
#include FT_FREETYPE_H

/**
 * @brief Access to an TTF font.
 *
 * This is a low-level class to allow acces to an TTF font
 *
 */
class FontData {

private:
    static FT_Library library_;

    bool initialized_{false};

public:
    FontData() {

        load();

        if (!initialized_) {
            log_e("Font data not recognized!");
        }
    }

    ~FontData() = default;

    [[nodiscard]] inline auto isInitialized() const -> bool { return initialized_; }
    [[nodiscard]] inline auto getLibrary() const -> FT_Library { return library_; }

    virtual auto getData() const -> MemoryPtr = 0;
    virtual auto getDataSize() const -> int = 0;

    virtual auto getPrivateData() const -> MemoryPtr = 0;
    virtual auto getPrivateDataSize() const -> int = 0;

    virtual auto ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const
        -> bool = 0;

    auto load() -> bool;
};
