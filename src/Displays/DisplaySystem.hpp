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
#include <Arduino.h>
#include <array>

// This isn't the best -- we may want to make this dynamically
// allocated instead in the object?
#define MAX_LINES_IN_SPLIT 100

#define INVERTED_DISPLAY true
// #define INVERTED_DISPLAY false

class DisplaySystem : public GFXcanvas1 {
public:
    DisplaySystem(int16_t w, int16_t h);
    ~DisplaySystem() override = default;

    // MARK: - Required virtual methods that subclasses must implement

    virtual void updateDisplay() = 0;

    // MARK: - Potentially overridable methods for configuring partial refresh

    virtual void setPartialRefresh();
    void unsetPartialRefresh();
    auto getPartialRefresh() -> int;
    virtual void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void setFullWindow();

    void invertDisplay(bool newInvert) override;
    void flipInversion();
    auto getInversion() -> bool { return invertedDisplay_; }

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

    // MARK: - Text rendering code

    auto getFont() -> const GFXfont *;
    void drawText(const String &newText, int x, int y, int width, int height);
    void drawTextInBox(const String &newText, int startX, int startY, int endX, int endY);
    void getTextBounds(const char *buffer, uint16_t *w, uint16_t *h);
    auto getTextHeight() -> int;
    auto getTextHeight(const char *buffer) -> int;
    auto getTextWidth(const char *buffer) -> int;

    // Convenience methods for drawing that take into account offsets
    void renderMaxBoxWithText(const String &text);
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

    // TODO: Eventually we'll probably want our draw commands to handle the offsets
    // internally without ever needing to expose them.
    int localXOffset = 0;
    int localYOffset = 0;
    int xMin = 0;
    int xMax = 0;
    int yMin = 0;
    int yMax = 0;
    int maxPixelsX = 0;
    int maxPixelsY = 0;

protected:
    void drawTextCmdsOnly(const String &newText, int x, int y, int lineHeight);
    void drawBoxCmdsOnly(int xStart, int yStart, int xEnd, int yEnd);
    void splitStringToLines(const String &stringToSplit,
                            std::array<String, MAX_LINES_IN_SPLIT> &lines, int &numLines);
    void rotate(uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h);
    void computeMinMax();

    int partialRefresh_ = 0;
    uint16_t partialX_, partialY_, partialW_, partialH_;

    bool autoUpdate_;

    float widthInMm_ = 0;
    float heightInMm_ = 0;
    float focusWidthInMm_;
    float focusHeightInMm_;

    bool invertedDisplay_ = false;
};

#endif