#include "SpiramAllocator.hpp"

auto operator==(const std::string &stdStr, const SpiramString &spiramStr) -> bool {
    return spiramStr.compare(stdStr.c_str()) == 0;
}

auto SpiZAlloc(void *opaque, size_t items, size_t size) -> void * {
    // log_d("SpiZAlloc: %d %d", (int)items, (int)size);
    return SpiRamMalloc(items * size);
}

void SpiZFree(void *opaque, void *address) {
    // log_d("SpiZFree");
    SpiRamFree(address);
}
