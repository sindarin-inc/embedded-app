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