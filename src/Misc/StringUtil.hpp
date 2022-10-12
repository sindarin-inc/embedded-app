#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "sindarin-debug.h"

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
    int sizeS = std::snprintf(nullptr, 0, format, args...);
    if (sizeS <= 0) {
        log_e("Failed to generate formatted string");
        return "";
    }
    auto size = sizeS + 1;
    auto buf = std::make_unique<char[]>(size);
    auto sizeF = std::snprintf(buf.get(), size, format, args...);
    if (sizeS != sizeF) {
        log_e("Formatted string was a different size %d vs %d", sizeS, sizeF);
    }
    // We don't want the '\0' inside
    return {buf.get(), buf.get() + size - 1};
}

#pragma clang diagnostic pop