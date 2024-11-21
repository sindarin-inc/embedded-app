#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "sindarin-debug.h"

// C++20 has std::format which could replace this
// From https://stackoverflow.com/a/26221725
// Or we could copy in the `fmt` library which is the same thing
// https://github.com/fmtlib/fmt

auto WordCount(const char *str) -> int;
auto BytesToHex(const uint8_t *data, size_t size, const char *separator = nullptr) -> std::string;

// Normally we would avoid these warnings with something like
//   __attribute__((format(printf, 1, 2)))
// But that currently doesn't with variadic templates like we're using here.
// The fix is coming. See: https://reviews.llvm.org/D112579
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

template <typename T, typename... ARGS>
auto StringFormatInternal(const char *format, ARGS... args) -> T {
    // Extra space for '\0'
    // This raises a warning about `format` not being a literal.
    int sizeS = std::snprintf(nullptr, 0, format, args...);
    if (sizeS <= 0) {
        log_e("Failed to generate formatted string: %s", format);
        return "";
    }
    auto size = sizeS + 1;
    auto buf = std::make_unique<char[]>(size);
    auto sizeF = std::snprintf(buf.get(), size, format, std::forward<ARGS>(args)...);
    if (sizeS != sizeF) {
        log_e("Formatted string was a different size %d vs %d", sizeS, sizeF);
    }
    // We don't want the '\0' inside
    return {buf.get(), buf.get() + size - 1};
}

#pragma GCC diagnostic pop
#pragma clang diagnostic pop

template <typename... ARGS>
auto StringFormat(const char *format, ARGS... args) -> std::string {
    return StringFormatInternal<std::string>(format, args...);
}

template <typename... ARGS>
auto SpiRamStringFormat(const char *format, ARGS... args) -> SpiramString {
    return StringFormatInternal<SpiramString>(format, args...);
}

auto SafeSnprintf(char *str, size_t size, const char *format, ...) -> bool;

auto HasSuffix(std::string const &str, std::string const &suffix) -> bool;

auto HasPrefix(const char *str, const char *pre, size_t maxToCompare = 0) -> bool;

auto Commify(uint32_t n) -> std::string;

auto CaseInsensitiveCompare(const std::string &str1, const std::string &str2) -> bool;

auto ReplaceAll(std::string str, const std::string &from, const std::string &to) -> std::string;