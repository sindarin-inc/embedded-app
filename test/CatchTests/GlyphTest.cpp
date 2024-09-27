#include <catch_amalgamated.hpp>

#include "Misc/Defer.hpp"
#include "UI/Screen.hpp"
#include "ViewTestCommon.hpp"
#include "sindarin-debug.h"

extern Font fontFace0;
extern Font fontFace1;
extern Font fontFace2;

class GlyphTestScreen : public Screen {
public:
    GlyphTestScreen(const std::vector<std::string> &glyphs, int gridWidth, int gridHeight)
        : glyphs(glyphs), gridWidth(gridWidth), gridHeight(gridHeight) {}

    void render(DisplaySystem &display) override {
        // In the past the display box defaulted to 5px insets
        auto width = display.width() - 10;
        auto height = display.height() - 10;

        int xSpacing = width / gridWidth;
        int ySpacing = height / gridHeight;

        int glyphsPerPage = gridWidth * gridHeight;
        int pageStartIndex = page * glyphsPerPage;
        for (int i = 0; i < glyphsPerPage; i++) {
            int index = pageStartIndex + i;
            if (index >= glyphs.size()) {
                break;
            }
            int x = (i % gridWidth) * xSpacing + 5;
            int y = (i / gridWidth) * ySpacing + 5;
            display.drawSingleLineOfText(glyphs[pageStartIndex + i], x, y);
        }

        display.updateDisplay();
    }

    void processCommand(RemoteCommand command) override{};

    auto pages() -> int { return (glyphs.size() / (gridWidth * gridHeight)) + 1; }
    void setPage(int page) { this->page = page; }

private:
    std::vector<std::string> glyphs;
    int gridWidth;
    int gridHeight;
    int page = 0;
};

TEST_CASE("Test glyph rendering") {
    auto display = createDisplay();

    std::vector<std::string> glyphs = {
        "0",  "1", "2", "3", "4", "5", "6",  "7", "8", "9", "a", "b", "c", "d", "e", "f",
        "g",  "h", "i", "j", "k", "l", "m",  "n", "o", "p", "q", "r", "s", "t", "u", "v",
        "w",  "x", "y", "z", "A", "B", "C",  "D", "E", "F", "G", "H", "I", "J", "K", "L",
        "M",  "N", "O", "P", "Q", "R", "S",  "T", "U", "V", "W", "X", "Y", "Z", "á", "é",
        "í",  "ó", "ú", "ü", "ñ", "ß", "ç",  "Á", "É", "Í", "Ó", "Ú", "Ü", "Ñ", "Ç", "!",
        "\"", "#", "$", "%", "&", "'", "(",  ")", "*", "+", ",", "-", ".", "/", ":", ";",
        "<",  "=", ">", "?", "@", "[", "\\", "]", "^", "_", "`", "{", "|", "}", "~"};

    std::vector<std::string> glyphPairs;
    for (const auto &glyph1 : glyphs) {
        for (const auto &glyph2 : glyphs) {
            glyphPairs.push_back(glyph1 + glyph2);
        }
    }

    auto codePointToUtf8 = [](int code) -> std::string {
        std::string glyph;

        // Convert code point to UTF-8
        if (code < 0x80) {
            // 1-byte sequence
            glyph += static_cast<char>(code);
        } else if (code < 0x800) {
            // 2-byte sequence
            glyph += static_cast<char>(0xC0 | (code >> 6));
            glyph += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            // 3-byte sequence
            glyph += static_cast<char>(0xE0 | (code >> 12));
            glyph += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            glyph += static_cast<char>(0x80 | (code & 0x3F));
        }
        return glyph;
    };

    auto generateGlyphsInRange = [&codePointToUtf8](int start,
                                                    int end) -> std::vector<std::string> {
        std::vector<std::string> glyphs;
        for (int code = start; code <= end; ++code) {
            std::string glyph = codePointToUtf8(code);
            glyphs.push_back(glyph);
        }
        return glyphs;
    };

    std::vector<std::string> latinGlyphs = generateGlyphsInRange(0x0020, 0x007F);
    std::vector<std::string> latin1SupplementGlyphs = generateGlyphsInRange(0x0080, 0x00FF);
    std::vector<std::string> latinExtendedAGlyphs = generateGlyphsInRange(0x0100, 0x017F);
    std::vector<std::string> latinExtendedBGlyphs = generateGlyphsInRange(0x0180, 0x024F);
    std::vector<std::string> spacingModifierLetters = generateGlyphsInRange(0x02B0, 0x02FF);
    std::vector<std::string> greekGlyphs = generateGlyphsInRange(0x0370, 0x03FF);
    std::vector<std::string> cyrillicGlyphs = generateGlyphsInRange(0x0400, 0x04FF);
    std::vector<std::string> generalPunctuationGlyphs = generateGlyphsInRange(0x2000, 0x206F);
    std::vector<std::string> superscriptsAndSubscriptsGlyphs =
        generateGlyphsInRange(0x2070, 0x209F);
    std::vector<std::string> currencySymbolsGlyphs = generateGlyphsInRange(0x20A0, 0x20CF);
    std::vector<std::string> userDefinedGlyphs = generateGlyphsInRange(0xE000, 0xE07C);
    std::vector<std::string> alphabeticPresentationFormsGlyphs =
        generateGlyphsInRange(0xFB00, 0xFB4F);

    auto renderGlyphs = [&display](std::vector<std::string> &glyphs, const std::string &key,
                                   int gridWidth, int gridHeight, Font *font) {
        GlyphTestScreen screen(glyphs, gridWidth, gridHeight);
        display->setFont(font);
        for (int i = 0; i < screen.pages(); i++) {
            screen.setPage(i);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED(display, key + std::to_string(i));
        }
    };

    auto renderAllGlyphs = [&](const std::string &key, int gridWidth, int gridHeight, Font *font) {
        renderGlyphs(glyphPairs, key + "-pairs", gridWidth, gridHeight, font);
        renderGlyphs(latinGlyphs, key + "-latin", gridWidth, gridHeight, font);
        renderGlyphs(latin1SupplementGlyphs, key + "-latin1Supplement", gridWidth, gridHeight,
                     font);
        renderGlyphs(latinExtendedAGlyphs, key + "-latinExtendedA", gridWidth, gridHeight, font);
        renderGlyphs(latinExtendedBGlyphs, key + "-latinExtendedB", gridWidth, gridHeight, font);
        renderGlyphs(spacingModifierLetters, key + "-spacingModifierLetters", gridWidth, gridHeight,
                     font);
        renderGlyphs(greekGlyphs, key + "-greek", gridWidth, gridHeight, font);

        renderGlyphs(cyrillicGlyphs, key + "-cyrillic", gridWidth, gridHeight, font);
        renderGlyphs(generalPunctuationGlyphs, key + "-generalPunctuation", gridWidth, gridHeight,
                     font);
        renderGlyphs(superscriptsAndSubscriptsGlyphs, key + "-superscriptsAndSubscripts", gridWidth,
                     gridHeight, font);
        renderGlyphs(currencySymbolsGlyphs, key + "-currencySymbols", gridWidth, gridHeight, font);
        renderGlyphs(userDefinedGlyphs, key + "-userDefined", gridWidth, gridHeight, font);
        renderGlyphs(alphabeticPresentationFormsGlyphs, key + "-alphabeticPresentationForms",
                     gridWidth, gridHeight, font);
    };

    SECTION("Test small font Glyphs") {
        renderAllGlyphs("GlyphTests/GlyphTestS", 12, 16, static_cast<Font *>(&fontFace0));
    }

    SECTION("Test medium font Glyphs") {
        renderAllGlyphs("GlyphTests/GlyphTestM", 10, 14, static_cast<Font *>(&fontFace1));
    }

    SECTION("Test large font Glyphs") {
        renderAllGlyphs("GlyphTests/GlyphTestL", 8, 12, static_cast<Font *>(&fontFace2));
    }
}
