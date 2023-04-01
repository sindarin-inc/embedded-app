#include "SpiramAllocator.hpp"

void *spiRamMalloc(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
