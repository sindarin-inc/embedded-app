#pragma once

#include <ArduinoJson.h>

#include "Misc/SpiramAllocator.hpp"

#if SIMULATOR

using SpiRamJsonDocument = DynamicJsonDocument;

#else

// Allocator to use the SPI RAM for JSON document
// See: https://arduinojson.org/v6/api/basicjsondocument/
struct SpiRamAllocator {
    auto allocate(size_t size) -> void * { return SpiRamMalloc(size); }

    void deallocate(void *pointer) { SpiRamFree(pointer); }

    auto reallocate(void *ptr, size_t newSize) -> void * { return SpiRamRealloc(ptr, newSize); }
};

using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

#endif

auto DumpJson(const JsonVariant &v) -> std::string;
