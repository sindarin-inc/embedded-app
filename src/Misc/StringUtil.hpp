#pragma once

#include <memory>
#include <stdexcept>
#include <string>

// C++20 has std::format which could replace this
// From https://stackoverflow.com/a/26221725
template <typename... ARGS>
auto StringFormat(const std::string &format, ARGS... args) -> std::string {
    // Extra space for '\0'
    int sizeS = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (sizeS <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    auto size = static_cast<size_t>(sizeS);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    // We don't want the '\0' inside
    return {buf.get(), buf.get() + size - 1};
}
