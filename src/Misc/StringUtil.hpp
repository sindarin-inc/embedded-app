#pragma once

#include <memory>
#include <stdexcept>
#include <string>

// C++20 has std::format which could replace this
// From https://stackoverflow.com/a/26221725
// Or we could copy in the `fmt` library which is the same thing
// https://github.com/fmtlib/fmt
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"

auto WordCount(const char *str) -> int;

template <typename... ARGS>
auto StringFormat(const char *format, ARGS... args) -> std::string {
    // Extra space for '\0'
    // This raises a warning about `format` not being a literal.
    int sizeS = std::snprintf(nullptr, 0, format, args...) + 1;
    if (sizeS <= 0) {
        abort();
    }
    auto size = static_cast<size_t>(sizeS);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format, args...);
    // We don't want the '\0' inside
    return {buf.get(), buf.get() + size - 1};
}

#pragma clang diagnostic pop