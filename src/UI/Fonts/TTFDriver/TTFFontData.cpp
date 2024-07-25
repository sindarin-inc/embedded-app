#include "config.h"

#if CONFIG_FONT_TTF

#include "TTFFontData.hpp"
#include "esp_heap_caps.h"
#include "freetype/ftmodapi.h"

FT_Library FontData::library = nullptr;

#if FT_USE_PSRAM

static auto myFtAlloc(FT_Memory memory, long size) -> void * {
    void *mem = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (mem == nullptr) {
        log_e("Failed to allocate SPIRAM memory for FreeType library.");
    }
    return mem;
}

static auto myFtRealloc(FT_Memory memory, long currSize, long newSize, void *block) -> void * {
    void *mem = heap_caps_realloc(block, static_cast<size_t>(newSize), MALLOC_CAP_SPIRAM);
    if (mem == nullptr) {
        log_e("Failed to reallocate SPIRAM memory for FreeType library.");
    }
    return mem;
}

static void myFtFree(FT_Memory memory, void *block) { heap_caps_free(block); }

static FT_MemoryRec_ ftMemory = {
    nullptr,
    myFtAlloc,
    myFtFree,
    myFtRealloc,
};

#endif

auto FontData::load() -> bool {

    initialized_ = false;

    if (library == nullptr) {
// int error = FT_Init_FreeType(&library);
#if FT_USE_PSRAM
        FT_Error error = FT_New_Library(&ftMemory, &library);
        if (error) {
            log_e("An error occurred during FreeType library initialization.");
        }
        FT_Add_Default_Modules(library);
#else
        FT_Error error = FT_Init_FreeType(&library);
        if (error) {
            log_e("An error occurred during FreeType library initialization.");
        }
#endif
        FT_Bool cacheEnabled;
        FT_Property_Get(library, "truetype", "cache-sfnt-objects", &cacheEnabled);

        log_d("FreeType library initialized. Cache SFNT objects: %s",
              cacheEnabled ? "enabled" : "disabled");
    }

    initialized_ = true;

    return true;
}

#endif