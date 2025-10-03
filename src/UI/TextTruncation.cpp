
#include "TextTruncation.hpp"

auto Utf8Substr(const std::string &str, unsigned int start, size_t leng) -> std::string {
    if (leng == 0) {
        return "";
    }
    unsigned int c, i, ix, q;
    auto min = std::string::npos;
    auto max = std::string::npos;
    for (q = 0, i = 0, ix = str.length(); i < ix; i++, q++) {
        if (q == start) {
            min = i;
        }
        if (q <= start + leng || leng == std::string::npos) {
            max = i;
        }

        c = static_cast<unsigned char>(str[i]);
        if (
            // c>=0   &&
            c <= 127) {
            i += 0;
        } else if ((c & 0xE0) == 0xC0) {
            i += 1;
        } else if ((c & 0xF0) == 0xE0) {
            i += 2;
        } else if ((c & 0xF8) == 0xF0) {
            i += 3;
            // else if (($c & 0xFC) == 0xF8) i+=4; // 111110bb //byte 5, unnecessary in 4 byte UTF-8
            // else if (($c & 0xFE) == 0xFC) i+=5; // 1111110b //byte 6, unnecessary in 4 byte UTF-8
        } else {
            return ""; // invalid utf8
        }
    }
    if (q <= start + leng || leng == std::string::npos) {
        max = i;
    }
    if (min == std::string::npos || max == std::string::npos) {
        return "";
    }
    return str.substr(min, max);
}

auto TruncateStringToFitBinarySearchImpl(DisplaySystem &display, Font *font, const std::string &str,
                                         uint16_t maxWidth, uint16_t &initialWidth) -> std::string {
    uint16_t w, h;

    // Unicode ellipsis character
    const std::string ellipsis = "…"; // Unicode ellipsis character
    uint16_t ellipsisWidth, ellipsisHeight;
    display.getTextSize(ellipsis, &ellipsisWidth, &ellipsisHeight, font);

    int left = 0;
    int right = str.size() - 1;
    int charsToShow = 0;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        display.getTextSize(Utf8Substr(str, 0, mid + 1), &w, &h, font);
        int currentWidth = w;

        if (currentWidth + ellipsisWidth <= maxWidth) {
            left = mid + 1;
            charsToShow = left;
        } else {
            right = mid - 1;
        }
    }

    return Utf8Substr(str, 0, charsToShow) + ellipsis;
}

// Truncate string by estimating the cut point and iterating from there
auto TruncateStringToFitEstimateImpl(DisplaySystem &display, Font *font, const std::string &str,
                                     uint16_t maxWidth, uint16_t &initialWidth) -> std::string {
    uint16_t w, h;

    const std::string ellipsis = "…"; // Unicode ellipsis character
    uint16_t ellipsisWidth, ellipsisHeight;
    display.getTextSize(ellipsis, &ellipsisWidth, &ellipsisHeight, font);

    // Make an educated guess about how many characters we can fit in the space
    int estimatedEnd = static_cast<int>(str.size()) * maxWidth / initialWidth - 2;
    display.getTextSize(Utf8Substr(str, 0, estimatedEnd), &w, &h, font);
    // Search up or down until we find the right size
    bool up = w + ellipsisWidth < maxWidth;

    while (true) {
        if (up) {
            estimatedEnd++;
        } else {
            estimatedEnd--;
        }
        display.getTextSize(Utf8Substr(str, 0, estimatedEnd), &w, &h, font);
        w += ellipsisWidth;

        if (up && w > maxWidth) {
            // Too big, back off one character
            estimatedEnd--;
            break;
        };
        if (!up && w < maxWidth) {
            break;
        };
    }

    return Utf8Substr(str, 0, estimatedEnd) + ellipsis;
}

auto TruncateStringToFit(DisplaySystem &display, Font *font, const std::string &str,
                         uint16_t maxWidth, uint16_t &initialWidth) -> std::string {
    return TruncateStringToFitEstimateImpl(display, font, str, maxWidth, initialWidth);
}
