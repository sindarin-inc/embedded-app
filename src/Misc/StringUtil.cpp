#include "StringUtil.hpp"

auto WordCount(const char *str) -> int {
    int wordCount = 0;
    const char *s = str;
    bool newWord = true;
    while (*s) {
        if (isspace(*s)) {
            newWord = true;
        } else if (newWord) {
            wordCount++;
            newWord = false;
        }
        s++;
    }
    return wordCount;
}

// bytesToHex
// https://stackoverflow.com/a/3386294
auto BytesToHex(const uint8_t *data, size_t size) -> std::string {
    std::string result;
    for (size_t i = 0; i < size; ++i) {
        char buf[3];
        // NOLINTNEXTLINE(cert-err33-c) -- this is a safe format string
        snprintf(buf, sizeof(buf), "%02x", data[i]);
        result += buf;
    }
    return result;
}

auto SafeSnprintf(char *str, size_t size, const char *format, ...) // NOLINT(cert-dcl50-cpp)
    -> bool {
    va_list args;
    va_start(args, format);
    int res = vsnprintf(str, size, format, args);
    va_end(args);

    // check if entire string was written
    if (res >= 0 && res < size) {
        return true;
    }

    // unable to write entire string
    return false;
}

auto HasSuffix(std::string const &str, std::string const &suffix) -> bool {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}
