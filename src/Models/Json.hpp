#pragma once

#include <ArduinoJson.h>
#include <Spiram/Memory.hpp>

#if SIMULATOR

using SpiRamJsonDocument = DynamicJsonDocument;

#else

// Allocator to use the SPI RAM for JSON document
// See: https://arduinojson.org/v6/api/basicjsondocument/
struct SpiramArduinoJsonAllocator {
    auto allocate(size_t size) -> void * { return SpiramMalloc(size); }

    void deallocate(void *pointer) { SpiramFree(pointer); }

    auto reallocate(void *ptr, size_t newSize) -> void * { return SpiramRealloc(ptr, newSize); }
};

using SpiRamJsonDocument = BasicJsonDocument<SpiramArduinoJsonAllocator>;

#endif

auto DumpJson(const JsonVariant &v) -> std::string;
