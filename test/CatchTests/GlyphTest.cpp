#include <catch_amalgamated.hpp>

#include "Misc/Defer.hpp"
#include "UI/Screen.hpp"
#include "ViewTestCommon.hpp"
#include "sindarin-debug.h"

extern bool FakeEinkET013TT1SkipWaveform;
extern bool FakeEinkET013TT1SkipSignalling;

class GlyphTestScreen : public Screen {
public:
    GlyphTestScreen(const std::vector<std::string> &glyphs, int gridWidth, int gridHeight)
        : glyphs(glyphs), gridWidth(gridWidth), gridHeight(gridHeight) {}

    void render(DisplaySystem &display) override {
        int xSpacing = display.width() / gridWidth;
        int ySpacing = display.height() / gridHeight;

        int glyphsPerPage = gridWidth * gridHeight;
        int pageStartIndex = page * glyphsPerPage;
        for (int i = 0; i < glyphsPerPage; i++) {
            int index = pageStartIndex + i;
            if (index >= glyphs.size()) {
                break;
            }
            int x = (i % gridWidth) * xSpacing;
            int y = (i / gridWidth) * ySpacing;
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
    display->setBoundingBox({0, 0, static_cast<uint32_t>(display->defaultScreenWidth),
                             static_cast<uint32_t>(display->defaultScreenHeight)});

    FakeEinkET013TT1SkipSignalling = true;
    FakeEinkET013TT1SkipWaveform = true;
    defer {
        FakeEinkET013TT1SkipWaveform = false;
        FakeEinkET013TT1SkipSignalling = false;
    };

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

    auto renderGlyphs =
        [&glyphPairs, &display](const std::string &key, int gridWidth, int gridHeight, Font *font) {
        GlyphTestScreen screen(glyphPairs, gridWidth, gridHeight);
        display->setFont(font);
        for (int i = 0; i < screen.pages(); i++) {
            screen.setPage(i);
            screen.render(*display);
            ASSERT_VIEW_UNCHANGED(key + std::to_string(i));
        }
    };

    SECTION("Test small font Glyphs") {
        renderGlyphs("GlyphTests/GlyphTestS", 12, 16, display->style.solSansSmall.readableFont);
    }

    SECTION("Test medium font Glyphs") {
        renderGlyphs("GlyphTests/GlyphTestM", 10, 14, display->style.solSansMedium.readableFont);
    }

    SECTION("Test large font Glyphs") {
        renderGlyphs("GlyphTests/GlyphTestL", 8, 12, display->style.solSansLarge.readableFont);
    }

    FakeEinkET013TT1SkipSignalling = false;
}
