#pragma once

#include <cstdlib>
#include <esp_heap_caps.h>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

template <typename T>
struct SpiramAllocator {
    using value_type = T;

    SpiramAllocator() = default;

    template <typename U>
    SpiramAllocator(const SpiramAllocator<U> &) {}

    auto allocate(std::size_t n) -> T * {
        return reinterpret_cast<T *>(heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM));
    }

    auto allocateShared() -> std::shared_ptr<T> {
        T *tp = allocate(1);
        assert(tp);
        T *p = new (tp) T();
        return std::shared_ptr<T>(p, &heap_caps_free);
    }

    void deallocate(T *p, std::size_t n) { heap_caps_free((void *)p); }
};

template <typename T>
auto SpiRamPtr() -> std::shared_ptr<T> {
    static SpiramAllocator<T> allocator;
    return allocator.allocateShared();
}

template <typename T, typename U>
auto operator==(const SpiramAllocator<T> &, const SpiramAllocator<U> &) -> bool {
    return true;
}

template <typename T, typename U>
auto operator!=(const SpiramAllocator<T> &, const SpiramAllocator<U> &) -> bool {
    return false;
}

typedef std::basic_string<char, std::char_traits<char>, SpiramAllocator<char>> SpiramString;
auto operator==(const SpiramString &spiramStr, const std::string &stdStr) -> bool;

// For some reason in the sim (and on idf only on linux) the compiler is dumb and can't derive these
// from the equality operator. I think the <=> (spaceship operator) in c++ is supposed to fix this.
auto operator!=(const SpiramString &spiramStr, const std::string &stdStr) -> bool;
auto operator==(const std::string &stdStr, const SpiramString &spiramStr) -> bool;
auto operator!=(const std::string &stdStr, const SpiramString &spiramStr) -> bool;

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
void SpiRamFree(void *ptr);

template <typename T>
auto CustomArrayPtr(size_t len, void *(*mallocFunc)(size_t), void (*freeFunc)(void *))
    -> std::shared_ptr<T[]> {
    return std::shared_ptr<T[]>((T *)mallocFunc(len * sizeof(T)), CustomDeleter<T>(freeFunc));
}

template <typename T>
auto SpiRamArrayPtr(size_t len) -> std::shared_ptr<T[]> {
    return CustomArrayPtr<T>(len, &SpiRamMalloc, &heap_caps_free);
}

template <typename T>
using SpiramVector = std::vector<T, SpiramAllocator<T>>;

class SpiramStringBuf : public std::streambuf {
public:
    SpiramStringBuf(const SpiramString &str) {
        char *begin = const_cast<char *>(str.data());
        char *end = begin + str.size();
        setg(begin, begin, end);
    }

protected:
    auto seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which)
        -> pos_type override {
        if (dir == std::ios_base::beg) {
            setg(eback(), eback() + off, egptr());
        } else if (dir == std::ios_base::cur) {
            setg(eback(), gptr() + off, egptr());
        } else if (dir == std::ios_base::end) {
            setg(eback(), egptr() + off, egptr());
        }
        return gptr() - eback();
    }

    auto seekpos(pos_type pos, std::ios_base::openmode which) -> pos_type override {
        return seekoff(pos, std::ios_base::beg, which);
    }
};

class SpiramIStream : public std::istream {
public:
    SpiramIStream(SpiramString str) : std::istream(&buf_), str_(std::move(str)), buf_(str_) {}

private:
    SpiramString str_;
    SpiramStringBuf buf_;
};
