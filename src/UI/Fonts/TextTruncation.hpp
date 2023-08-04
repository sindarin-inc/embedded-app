#include <string>

#include "Displays/DisplaySystem.hpp"
#include "UI/Fonts/Font.hpp"

/**
 * Truncate a string and add an ellipsis to the end to fit in a given width.
 *
 * @param display DisplaySystem (to get the text size)
 * @param font Font to use
 * @param str The text to truncate
 * @param maxWidth The maximum width of the text + ellipsis
 * @param initialWidth The initial width of the text, also returns the final width of the text.
 * @return The truncated string + ellipsis
 */

auto TruncateStringToFit(DisplaySystem &display, Font *font, const std::string &str,
                         uint16_t maxWidth, uint16_t &initialWidth) -> std::string;
