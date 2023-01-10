#pragma once

#include <cstdlib>
#include <esp_heap_caps.h>
#include <memory>

template <typename T>
struct SpiramAllocator {
    using value_type = T;

    SpiramAllocator() = default;

    template <typename U>
    SpiramAllocator(const SpiramAllocator<U> &) {}

    auto allocate(std::size_t n) -> T * {
        return reinterpret_cast<T *>(heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM));
    }

    void deallocate(T *p, std::size_t n) { heap_caps_free((void *)p); }
};

template <typename T, typename U>
auto operator==(const SpiramAllocator<T> &, const SpiramAllocator<U> &) -> bool {
    return true;
}

template <typename T, typename U>
auto operator!=(const SpiramAllocator<T> &, const SpiramAllocator<U> &) -> bool {
    return false;
}
