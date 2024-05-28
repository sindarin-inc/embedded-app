#pragma once

#include "config.h"

#if TTF_SUPPORT

#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "TTFDefs.hpp"

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
    static constexpr char const *TAG = "TTFFontData";

    static FT_Library library_;

    bool initialized_{false};

    MemoryPtr data_{nullptr};
    uint32_t dataLength_{0};
    const std::unordered_map<uint32_t, int16_t> *kerns_;
    const std::unordered_map<uint32_t, uint16_t> *ligatures_;

public:
    FontData(const uint8_t *data, uint32_t length,
             const std::unordered_map<uint32_t, int16_t> *kerns,
             const std::unordered_map<uint32_t, uint16_t> *ligatures) noexcept
        : data_(const_cast<MemoryPtr>(data)), dataLength_(length), kerns_(kerns),
          ligatures_(ligatures) {

        load();

        if (!initialized_) {
            log_e("Font data not recognized!");
        }
    }

    FontData() = default;
    ~FontData() = default;

    [[nodiscard]] inline auto isInitialized() const -> bool { return initialized_; }
    [[nodiscard]] inline auto getLibrary() const -> FT_Library { return library_; }
    [[nodiscard]] inline auto getData() const -> MemoryPtr { return data_; }
    [[nodiscard]] inline auto getDataLength() const -> uint32_t { return dataLength_; }
    [[nodiscard]] inline auto getKernsData() const { return kerns_; }
    [[nodiscard]] inline auto getLigaturesData() const { return ligatures_; }

    auto load() -> bool;
};

#endif