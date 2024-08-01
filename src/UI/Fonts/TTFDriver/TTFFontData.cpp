#include "config.h"

#if CONFIG_FONT_TTF

#include "TTFFontData.hpp"

FT_Library FontData::library_ = nullptr;

auto FontData::load() -> bool {

    initialized_ = false;

    if (library_ == nullptr) {
        int error = FT_Init_FreeType(&library_);
        if (error) {
            log_e("An error occurred during FreeType library initialization.");
        }
    }

    initialized_ = true;

    return true;
}

#endif