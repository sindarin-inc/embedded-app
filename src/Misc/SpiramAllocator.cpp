#include "SpiramAllocator.hpp"

auto SpiRamMalloc(size_t size) -> void * { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }

bool operator==(const SpiramString &spiramStr, const std::string &stdStr) {
    return spiramStr.compare(stdStr.c_str()) == 0;
}
