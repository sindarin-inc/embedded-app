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

// This isn't the best -- we may want to make this dynamically
// allocated instead in the object?
#define MAX_LINES_IN_SPLIT 100

class DisplaySystem : public Adafruit_GFX {
public:
    auto getX() -> int;
    auto getY() -> int;
    auto getWidth() -> float;
    auto getHeight() -> float;

    void setX(int newX);
    void setY(int newY);

    //! Size of the focused area in the optics in mm
    void setFocusSize(float width, float height);
    auto getFocusWidth() -> float { return focusWidthInMm_; }
    auto getFocusHeight() -> float { return focusHeightInMm_; }

    void setWidth(float newWidth);
    void setHeight(float newHeight);

    void setText(const String &newText);

    auto getFont() -> const GFXfont *;

    void setCoordinates(int x, int y);

    virtual void setPartialRefresh();
    virtual void unsetPartialRefresh();
    virtual auto getPartialRefresh() -> int;

    virtual auto getTextHeight() -> int = 0;
    virtual auto getTextWidth(const char *buffer) -> int = 0;

    virtual void drawBox(int xStart, int yStart, int xEnd, int yEnd) = 0;

    virtual void drawText() = 0;
    virtual void drawText(String newText) = 0;
    virtual void drawText(String newText, int x, int y) = 0;

    virtual void drawTextInBox(String newText, int startX, int startY, int endX, int endY) = 0;

    virtual void drawBigWordInBox(String newText, int startX, int startY, int endX, int endY) = 0;

    DisplaySystem(int16_t w, int16_t h);
    ~DisplaySystem() = default;

protected:
    // Number of pixels in the X direction
    int x_;
    // Number of pixels in the Y direction
    int y_;

    float focusWidthInMm_;
    float focusHeightInMm_;

    float widthInMm_ = 0;
    float heightInMm_ = 0;

    int xToDraw_ = 0;
    int yToDraw_ = 0;

    String stringToDraw_;

    void splitStringToLines();

    int partialRefresh_ = 0;

    int numLines_ = 0;
    String lines_[MAX_LINES_IN_SPLIT];
};

#endif