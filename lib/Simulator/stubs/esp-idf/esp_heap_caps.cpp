#include "esp_heap_caps.h"

#include <cinttypes>
#include <memory>

void *heap_caps_malloc(size_t size, uint32_t caps) { return malloc(size); }

void heap_caps_free(void *ptr) { free(ptr); }
