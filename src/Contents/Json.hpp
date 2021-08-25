#include <ArduinoJson.h>

// Allocator to use the SPI RAM for JSON document
// See: https://arduinojson.org/v6/api/basicjsondocument/
struct SpiRamAllocator {
  void* allocate(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  }

  void deallocate(void* pointer) {
    heap_caps_free(pointer);
  }

  void* reallocate(void* ptr, size_t new_size) {
    return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
  }
};

typedef BasicJsonDocument<SpiRamAllocator> SpiRamJsonDocument;
