#pragma once

#include <cstdlib>
#include <esp_heap_caps.h>
#include <functional>
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

typedef std::basic_string<char, std::char_traits<char>, SpiramAllocator<char>> SpiramString;

template <typename T>
class CustomDeleter {
public:
    using FreeFuncPtr = void (*)(void *);

    explicit CustomDeleter(FreeFuncPtr freeFunc) : freeFunc_(freeFunc) {}

    void operator()(T *d) const { freeFunc_(d); }

private:
    FreeFuncPtr freeFunc_;
};

// It's heap_caps_malloc but with always uses MALLOC_CAP_SPIRAM to have a malloc-compatible
// interface.
auto SpiRamMalloc(size_t size) -> void *;

template <typename T>
auto CustomArrayPtr(size_t len, void *(*mallocFunc)(size_t), void (*freeFunc)(void *))
    -> std::shared_ptr<T[]> {
    return std::shared_ptr<T[]>((T *)mallocFunc(len * sizeof(T)), CustomDeleter<T>(freeFunc));
}

template <typename T>
auto SpiRamArrayPtr(size_t len) -> std::shared_ptr<T[]> {
    return CustomArrayPtr<T>(len, &SpiRamMalloc, &heap_caps_free);
}
