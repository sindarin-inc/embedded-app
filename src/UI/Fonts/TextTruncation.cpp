
#include "TextTruncation.hpp"

auto TruncateStringToFitBinarySearchImpl(DisplaySystem &display, Font *font, const std::string &str,
                                         uint16_t maxWidth, uint16_t &initialWidth) -> std::string {
    uint16_t w, h;

    Font *oldFont = display.getFont();
    display.setFont(font);

    // Unicode ellipsis character
    const std::string ellipsis = "…"; // Unicode ellipsis character
    uint16_t ellipsisWidth, ellipsisHeight;
    display.getTextSize(ellipsis, &ellipsisWidth, &ellipsisHeight);

    int left = 0;
    int right = str.size() - 1;
    int charsToShow = 0;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        display.getTextSize(str.substr(0, mid + 1), &w, &h);
        int currentWidth = w;

        if (currentWidth + ellipsisWidth <= maxWidth) {
            left = mid + 1;
            charsToShow = left;
        } else {
            right = mid - 1;
        }
    }

    display.setFont(oldFont);

    return str.substr(0, charsToShow) + ellipsis;
}

// Truncate string by estimating the cut point and iterating from there
auto TruncateStringToFitEstimateImpl(DisplaySystem &display, Font *font, const std::string &str,
                                     uint16_t maxWidth, uint16_t &initialWidth) -> std::string {
    uint16_t w, h;

    Font *oldFont = display.getFont();
    display.setFont(font);

    const std::string ellipsis = "…"; // Unicode ellipsis character
    uint16_t ellipsisWidth, ellipsisHeight;
    display.getTextSize(ellipsis, &ellipsisWidth, &ellipsisHeight);

    // Make an educated guess about how many characters we can fit in the space
    int estimatedEnd = (int)str.size() * maxWidth / initialWidth - 2;
    display.getTextSize(str.substr(0, estimatedEnd), &w, &h);
    // Search up or down until we find the right size
    bool up = w + ellipsisWidth < maxWidth;

    while (true) {
        if (up) {
            estimatedEnd++;
        } else {
            estimatedEnd--;
        }
        display.getTextSize(str.substr(0, estimatedEnd), &w, &h);
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

    display.setFont(oldFont);

    return str.substr(0, estimatedEnd) + ellipsis;
}

auto TruncateStringToFit(DisplaySystem &display, Font *font, const std::string &str,
                         uint16_t maxWidth, uint16_t &initialWidth) -> std::string {
    return TruncateStringToFitEstimateImpl(display, font, str, maxWidth, initialWidth);
}
