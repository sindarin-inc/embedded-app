#include "SpiramAllocator.hpp"

auto SpiRamMalloc(size_t size) -> void * { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
void SpiRamFree(void *ptr) { heap_caps_free(ptr); }

auto operator==(const SpiramString &spiramStr, const std::string &stdStr) -> bool {
    return spiramStr.compare(stdStr.c_str()) == 0;
}

auto operator!=(const SpiramString &spiramStr, const std::string &stdStr) -> bool {
    return spiramStr.compare(stdStr.c_str()) != 0;
}

auto operator==(const std::string &stdStr, const SpiramString &spiramStr) -> bool {
    return spiramStr.compare(stdStr.c_str()) == 0;
}

auto operator!=(const std::string &stdStr, const SpiramString &spiramStr) -> bool {
    return spiramStr.compare(stdStr.c_str()) != 0;
}
