#include "config.h"

#if CONFIG_FONT_TTF

#include "TTFFontData.hpp"
#include "esp_heap_caps.h"
#include "freetype/ftmodapi.h"

FT_Library FontData::library = nullptr;

static void *my_ft_alloc(FT_Memory memory, long size) {
    void *mem = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (mem == nullptr) {
        log_e("Failed to allocate SPIRAM memory for FreeType library.");
    }
    return mem;
}

static void *my_ft_realloc(FT_Memory memory, long currSize, long newSize, void *block) {
    void *mem = heap_caps_realloc(block, (size_t)newSize, MALLOC_CAP_SPIRAM);
    if (mem == nullptr) {
        log_e("Failed to reallocate SPIRAM memory for FreeType library.");
    }
    return mem;
}

static void my_ft_free(FT_Memory memory, void *block) { heap_caps_free(block); }

static FT_MemoryRec_ ft_memory = {
    nullptr,
    my_ft_alloc,
    my_ft_free,
    my_ft_realloc,
};

auto FontData::load() -> bool {

    initialized_ = false;

    if (library == nullptr) {
        // int error = FT_Init_FreeType(&library);
        FT_Error error = FT_New_Library(&ft_memory, &library);
        if (error) {
            log_e("An error occurred during FreeType library initialization.");
        }
        FT_Add_Default_Modules(library);
    }

    initialized_ = true;

    return true;
}

#endif