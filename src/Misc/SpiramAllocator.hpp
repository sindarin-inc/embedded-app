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

template <typename T>
using SpiramVector = std::vector<T, SpiramAllocator<T>>;

// Define a custom string stream buffer that uses your string type
class SpiramStringStreamBuf : public std::stringbuf {
public:
    SpiramStringStreamBuf() {
        // Set the internal buffer to use your custom string type
        char_type *buffer = SpiramAllocator<char>().allocate(1024);
        this->setp(buffer, buffer + 1024);
    }

    ~SpiramStringStreamBuf() {
        // Clean up the internal buffer
        // SpiramAllocator<char>().deallocate(this->pbase(), 1024);
    }

    SpiramString str() const {
        // Return the internal buffer as a SpiramString
        return SpiramString(this->pbase(), this->pptr() - this->pbase());
    }

    void str(const SpiramString &s) {
        // Set the internal buffer to the specified SpiramString
        // TODO: Don't copy
        char_type *buffer = SpiramAllocator<char>().allocate(s.length() + 1);
        std::copy(s.begin(), s.end(), buffer);
        buffer[s.length()] = '\0';
        this->setp(buffer, buffer + s.length());
    }
};

class SpiramStringStream : public std::stringstream {
public:
    SpiramStringStream() {
        // Set the string buffer to use your custom string type
        this->set_rdbuf(&buffer_);
    }

    explicit SpiramStringStream(const SpiramString &str = "") {
        // Set the string buffer to use your custom string type
        buffer_.str(str);
        this->set_rdbuf(&buffer_);
    }

    SpiramStringStreamBuf *rdbuf() const {
        // Return a pointer to the internal buffer
        return const_cast<SpiramStringStreamBuf *>(&buffer_);
    }

    ~SpiramStringStream() {
        // Clean up the string buffer
        delete this->rdbuf();
    }

protected:
    mutable SpiramStringStreamBuf buffer_;
};
