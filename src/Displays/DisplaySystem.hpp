// Superclass for various display systems
//
// Specific display should be derived from this class and should
// define their characteristics and use the underlying u8g2 libraries
// for drawing text and other things. This will allow the Renderer
// class to automatically use arbirary displays and optics systems
// without having to rewrite the Rendering code.
//
// When deriving from this class, add the new class header to
// the file "AllDisplays.hpp" so clients of these classes only
// need to include a single header to get all the different display
// options.
//

#ifndef DISPLAY_SYSTEM_H
#define DISPLAY_SYSTEM_H

#include <Adafruit_GFX.h>
#include <array>
#include <string>

#include "Misc/Style.hpp"

// This isn't the best -- we may want to make this dynamically
// allocated instead in the object?
#define MAX_LINES_IN_SPLIT 100

#define INVERTED_DISPLAY true
// #define INVERTED_DISPLAY false

class DisplaySystem {
public:
    DisplaySystem();
    ~DisplaySystem() = default;

    virtual void setup();

    Style style;

    // MARK: - Required virtual methods that subclasses must implement
    enum UpdateType {
        UNSPECIFIED,
        FULL,
        PARTIAL,
        FAST,
    };
    virtual void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED) = 0;

    // Use `gfxClass` when you need to get direct access to draw to the whole screen (potentially
    // outside of focus areas)
    [[nodiscard]] auto gfxClass() const -> Adafruit_GFX * { return gfxClass_; }

    // MARK: - Brought in from Adafruit_GFX
    // Use these methods for normal drawing as they'll take into account the offsets and focus area
    // automatically. Any methods we need from Adafruit_GFX that need to respect the focus area
    // should be copied in here.
    [[nodiscard]] auto width() const -> int16_t { return maxPixelsX_; };
    [[nodiscard]] auto height() const -> int16_t { return maxPixelsY_; };
    void fillScreen(uint16_t color) { gfxClass_->fillScreen(color); }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      uint16_t color) {
        gfxClass_->fillTriangle(xOffset(x0), yOffset(y0), xOffset(x1), yOffset(y1), xOffset(x2),
                                yOffset(y2), color);
    }
    void setFont(const GFXfont *f) {
        font_ = f;
        gfxClass_->setFont(f);
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        gfxClass_->fillRect(xOffset(x), yOffset(y), w, h, color);
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
        gfxClass_->drawLine(xOffset(x0), yOffset(y0), xOffset(x1), yOffset(y1), color);
    }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        gfxClass_->drawRect(xOffset(x), yOffset(y), w, h, color);
    }
    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
        gfxClass_->drawCircle(xOffset(x0), yOffset(y0), r, color);
    }
    void setCursor(int16_t x, int16_t y) { gfxClass_->setCursor(xOffset(x), yOffset(y)); }
    template <typename T>
    auto println(T in) -> size_t {
        return gfxClass_->println(in);
    }

    // MARK: - Potentially overridable methods for configuring partial refresh

    virtual void setPartialRefresh();
    void unsetPartialRefresh();
    auto getPartialRefresh() -> int;
    virtual void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void setFullWindow();

    void invertDisplay(bool newInvert);
    void flipInversion();
    auto getInversion() -> bool { return invertedDisplay_; }

    virtual void clearDisplay(){};

    // MARK: - AutoUpdate

    // Whether or not the display will immediately update after drawing operations. If this is set
    // to false, then updateDisplay must be called manually
    // TODO(john): I don't love this paradigm and I think it'd probably be better to just always be
    // explicit in our drawing code and call `updateDisplay` when we want to update the display.
    void setAutoUpdate(bool update);
    auto getAutoUpdate() -> bool;

    // MARK: - Physical optics stuff

    // Physical size of the display in mm
    auto getWidthInMm() -> float;
    auto getHeightInMm() -> float;

    // Size of the focused area in the optics in mm
    void setFocusSize(float width, float height);
    auto getFocusWidth() -> float { return focusWidthInMm_; }
    auto getFocusHeight() -> float { return focusHeightInMm_; }

    void setFocusOffset(float width, float height);
    auto getFocusOffsetWidth() -> float { return focusWidthOffsetInMm_; }
    auto getFocusOffsetHeight() -> float { return focusHeightOffsetInMm_; }

    // MARK: - Text rendering code

    auto getFont() -> const GFXfont *;
    void drawText(const std::string &newText, int x, int y, int width, int height);
    void drawTextInBox(const std::string &newText, int startX, int startY, int endX, int endY);
    void getTextBounds(const char *buffer, uint16_t *w, uint16_t *h);
    auto getTextHeight() -> int;
    auto getTextHeight(const char *buffer) -> int;
    auto getTextWidth(const char *buffer) -> int;

    // Convenience methods for drawing that take into account offsets
    void renderMaxBoxWithText(const std::string &text);
    void renderTextWithoutBox(const std::string &text);
    void renderBox(int xStart, int yStart, int xEnd, int yEnd);

    // MARK: - Graphic drawing code

    // Basically Adafruit_GFX::drawRect with the necessary partial refresh stuff
    // TODO: We might be able to instead just subclass `drawRect` here instead to be more consistent
    // with Adafruit_GFX
    void drawBox(int xStart, int yStart, int xEnd, int yEnd);
    // Draw a bitmap
    void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                     uint16_t color);

    // Clear the whole screen
    void clearScreen();

    // TODO: Just use `textcolor` and `textbgcolor` for these?
    uint16_t bg, fg;

    void setOffset(uint16_t xOffset, uint16_t yOffset) {
        localXOffset_ = xOffset;
        localYOffset_ = yOffset;
    }

protected:
    // Must be set in the constructor of subclasses
    Adafruit_GFX *gfxClass_;
    const GFXfont *font_;

    inline auto xOffset(int16_t value) -> int16_t { return value + localXOffset_ + xMin_; }
    inline auto yOffset(int16_t value) -> int16_t { return value + localYOffset_ + yMin_; }

    void drawTextCmdsOnly(const std::string &newText, int x, int y, int lineHeight);
    void drawBoxCmdsOnly(int xStart, int yStart, int xEnd, int yEnd);
    void splitStringToLines(const std::string &stringToSplit,
                            std::array<std::string, MAX_LINES_IN_SPLIT> &lines, int &numLines);
    void rotate(uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h);
    void computeMinMax();

    int partialRefresh_ = 0;
    uint16_t partialX_, partialY_, partialW_, partialH_;

    bool autoUpdate_;

    float widthInMm_ = 0;
    float heightInMm_ = 0;
    float focusWidthInMm_;
    float focusHeightInMm_;
    float focusWidthOffsetInMm_ = 0;
    float focusHeightOffsetInMm_ = 0;
    bool invertedDisplay_ = false;

    // TODO: We can probably simplify these
    int localXOffset_ = 0;
    int localYOffset_ = 0;
    int xMin_ = 0;
    int xMax_ = 0;
    int yMin_ = 0;
    int yMax_ = 0;
    int maxPixelsX_ = 0;
    int maxPixelsY_ = 0;
};

#endif