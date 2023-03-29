#pragma once

#include <ArduinoJson.h>

#include "esp_heap_caps.h"

#if SIMULATOR

using SpiRamJsonDocument = DynamicJsonDocument;

#else

// Allocator to use the SPI RAM for JSON document
// See: https://arduinojson.org/v6/api/basicjsondocument/
struct SpiRamAllocator {
    auto allocate(size_t size) -> void * { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }

    void deallocate(void *pointer) { heap_caps_free(pointer); }

    auto reallocate(void *ptr, size_t newSize) -> void * {
        return heap_caps_realloc(ptr, newSize, MALLOC_CAP_SPIRAM);
    }
};

using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

#endif

auto DumpJson(const JsonVariant &v) -> std::string;
