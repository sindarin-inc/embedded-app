#include "SpiramAllocator.hpp"

auto SpiRamMalloc(size_t size) -> void * { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
void SpiRamFree(void *ptr) { heap_caps_free(ptr); }

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const SpiramString &src, JsonVariant dst) -> bool {
    dst.set(src.c_str());
    return true;
}

auto operator==(const std::string &stdStr, const SpiramString &spiramStr) -> bool {
    return spiramStr.compare(stdStr.c_str()) == 0;
}
