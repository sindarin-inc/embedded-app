#include "SpiramAllocator.hpp"

auto SpiRamMalloc(size_t size) -> void * { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
void SpiRamFree(void *ptr) { heap_caps_free(ptr); }