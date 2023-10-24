#include <catch_amalgamated.hpp>

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

        int index = 0;
        for (int y = 0; y < gridHeight; ++y) {
            for (int x = 0; x < gridWidth; ++x) {
                if (index >= glyphs.size()) break;

                Point p;
                p.x = x * xSpacing;
                p.y = y * ySpacing;

                display.drawSingleLineOfText(glyphs[index], p.x, p.y);

                ++index;
            }
            if (index >= glyphs.size()) break;
        }

        display.updateDisplay();
    }

    void processCommand(RemoteCommand command) override{};

private:
    std::vector<std::string> glyphs;
    int gridWidth;
    int gridHeight;
};

TEST_CASE("Test glyph rendering") {
    // Just create and render a PowerDepletedScreen
    auto display = createDisplay();
    display->setBoundingBox({0, 0, static_cast<uint32_t>(display->defaultScreenWidth),
                             static_cast<uint32_t>(display->defaultScreenHeight)});

    FakeEinkET013TT1SkipSignalling = true;
    FakeEinkET013TT1SkipWaveform = true;

    std::vector<std::string> glyphs = {
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r",
        "s", "t", "u", "v", "w", "x", "y", "z", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
        "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "á", "é",
        "í", "ó", "ú", "ü", "ñ", "ß", "ç", "Á", "É", "Í", "Ó", "Ú", "Ü", "Ñ", "Ç"};

    SECTION("Test small font Glyphs") {
        GlyphTestScreen screen(glyphs, 16, 16);
        display->setFont(display->style.solSansSmall.readableFont);
        screen.render(*display);

        ASSERT_VIEW_UNCHANGED("GlyphTestScreenS");
    }

    SECTION("Test medium font Glyphs") {
        GlyphTestScreen screen(glyphs, 14, 14);
        display->setFont(display->style.solSansMedium.readableFont);
        screen.render(*display);

        ASSERT_VIEW_UNCHANGED("GlyphTestScreenM");
    }

    SECTION("Test large font Glyphs") {
        GlyphTestScreen screen(glyphs, 12, 12);
        display->setFont(display->style.solSansLarge.readableFont);
        screen.render(*display);

        ASSERT_VIEW_UNCHANGED("GlyphTestScreenL");
    }

    FakeEinkET013TT1SkipSignalling = false;
}
