// Superclass for various display systems
//
// Specific display should be derived from this class and should
// define their characteristics and use the underlying u8g2 libraries
// for drawing text and other things. This will allow the Renderer
// class to automatically use arbirary displays and optics systems
// without having to rewrite the Rendering code.
//

#ifndef DISPLAY_SYSTEM_H
#define DISPLAY_SYSTEM_H

#include <Adafruit_GFX.h>
#include <array>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <string>

#include "Displays/EinkTypes.hpp"
#include "Misc/StringUtil.hpp"
#include "Storage/FileSystem.hpp"
#include "UI/Fonts/Font.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "UI/Layout/Types.hpp"

struct DisplayInset {
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;

    [[nodiscard]] auto toString() const -> std::string {
        return StringFormat("t:%d, b:%d, l:%d, r:%d", top, bottom, left, right);
    }

    static constexpr auto zero() -> DisplayInset { return {0, 0, 0, 0}; }
};

struct DisplayRect {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;

    [[nodiscard]] auto toString() const -> std::string {
        return StringFormat("x:%d, y:%d, w:%d, h:%d", x, y, width, height);
    }

    [[nodiscard]] auto operator==(const DisplayRect &other) const -> bool {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }

    [[nodiscard]] auto operator!=(const DisplayRect &other) const -> bool {
        return !(*this == other);
    }

    static auto fromRect(SolRect rect) -> DisplayRect {
        return {static_cast<uint16_t>(rect.origin.x), static_cast<uint16_t>(rect.origin.y),
                static_cast<uint16_t>(rect.size.width), static_cast<uint16_t>(rect.size.height)};
    }

    static auto fromSizeAndInsets(uint16_t width, uint16_t height, DisplayInset inset)
        -> DisplayRect {
        return {inset.left, inset.top, static_cast<uint16_t>(width - inset.left - inset.right),
                static_cast<uint16_t>(height - inset.top - inset.bottom)};
    }

    [[nodiscard]] auto inset(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom) const
        -> DisplayRect {
        return {static_cast<uint16_t>(x + left), static_cast<uint16_t>(y + top),
                static_cast<uint16_t>(width - left - right),
                static_cast<uint16_t>(height - top - bottom)};
    }

    [[nodiscard]] auto inset(uint16_t xAmount, uint16_t yAmount) const -> DisplayRect {
        return inset(xAmount, xAmount, yAmount, yAmount);
    }
};

class DisplaySystem {
public:
    DisplaySystem();
    DisplaySystem(DisplaySystem const &) = delete;
    virtual ~DisplaySystem() = default;

    // Constant text-box inset defaults during reading. These should be overridden to sane values in
    // subclasses.
    [[nodiscard]] virtual auto defaultTextInsets() const -> DisplayInset = 0;
    [[nodiscard]] virtual auto minTextInsets() const -> DisplayInset = 0;
    [[nodiscard]] virtual auto minTextBoxWidth() const -> int16_t = 0;
    [[nodiscard]] virtual auto minTextBoxHeight() const -> int16_t = 0;
    [[nodiscard]] auto defaultTextRect() const -> DisplayRect {
        return DisplayRect::fromSizeAndInsets(width(), height(), defaultTextInsets());
    }

    // Constant suggested insets for UI (overridable if desired)
    [[nodiscard]] virtual auto defaultUIInsets() const -> DisplayInset {
        return defaultTextInsets();
    }

    virtual auto setup() -> bool;

    static void animationTimerCallback(TimerHandle_t timer);

    static TimerHandle_t animationTimer;

    bool mirrorToSerial = false;

    virtual void sendCurrentScreenToSerial(bool screenshot = false) const {}

    // MARK: - Required virtual methods that subclasses must implement
    enum UpdateType {
        UNSPECIFIED,
        FULL,
        PARTIAL,
        FAST,
        FAST_INDICATOR,
        FAST_ORIGINAL_AU4,
        ORIGINAL_PARTIAL,
        ERASE,
    };
    virtual void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                               bool clearFrameBuffer = true, int vcomNudge = 0,
                               DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                               ScreenSide screen = ScreenSide::Both,
                               SolRect altRect = {{0, 0}, {0, 0}}) = 0;

    virtual void beginPowerOn() {}

    // Use `gfxClass` when you need to get direct access to the underlying buffer
    [[nodiscard]] auto gfxClass() const -> Adafruit_GFX * { return gfxClass_; }

    // MARK: - Brought in from Adafruit_GFX
    // Use these methods for normal drawing as they'll take into account the offsets and focus area
    // automatically. Any methods we need from Adafruit_GFX that need to respect the focus area
    // should be copied in here.
    [[nodiscard]] auto width() const -> int16_t {
        return nativeWidth() - displayInsets().left - displayInsets().right;
    };
    [[nodiscard]] auto height() const -> int16_t {
        return nativeHeight() - displayInsets().top - displayInsets().bottom;
    };

    void fillScreen(uint16_t color) { gfxClass_->fillScreen(color); }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      uint16_t color) {
        gfxClass_->fillTriangle(xOffset(x0), yOffset(y0), xOffset(x1), yOffset(y1), xOffset(x2),
                                yOffset(y2), color);
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

    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
        gfxClass_->fillCircle(xOffset(x0), yOffset(y0), r, color);
    }

    void fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius,
                       uint16_t color) {
        gfxClass_->fillRoundRect(xOffset(x0), yOffset(y0), w, h, radius, color);
    }

    void drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius,
                       uint16_t color) {
        gfxClass_->drawRoundRect(xOffset(x0), yOffset(y0), w, h, radius, color);
    }

    void setRotation(uint8_t r) { gfxClass_->setRotation(r); }

    void setCursor(int16_t x, int16_t y) { gfxClass_->setCursor(xOffset(x), yOffset(y)); }
    template <typename T>
    auto println(T in) -> size_t {
        return gfxClass_->println(in);
    }

    // MARK: - Potentially overridable methods for configuring partial refresh

    virtual void setPartialRefresh();
    void unsetPartialRefresh();
    [[nodiscard]] auto getPartialRefresh() const -> int;
    virtual void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void setFullWindow();

    virtual void invertDisplay(bool newInvert);
    void flipInversion();
    [[nodiscard]] auto getInversion() const -> bool { return invertedDisplay_; }

    virtual void clearDisplay(){};
    virtual void clearFrameBuffer() { fillScreen(bg); };

    void forceDontChangeRefreshType() { forceRefreshType_ = true; }
    void clearDontChangeRefreshType() { forceRefreshType_ = false; }

    auto drawSingleLineOfText(const std::string &line, int x, int y, bool inverted = false,
                              Font *font = static_cast<Font *>(&fontFace0)) -> int;
    void drawSingleLineOfCenteredText(const std::string &line, bool inverted = false,
                                      Font *font = static_cast<Font *>(&fontFace0));

    // These getText* helpers were historically in this class, but they no longer need to be here
    // technically since the font is always passed in or it uses the default font.
    [[nodiscard]] auto defaultLineHeight() const -> int { return fontFace0.lineHeight(); }
    void getTextSize(const std::string &buffer, uint16_t *w, uint16_t *h,
                     Font *font = static_cast<Font *>(&fontFace0));
    auto getTextHeight(const std::string &buffer, Font *font = static_cast<Font *>(&fontFace0))
        -> int;
    auto getTextWidth(const std::string &buffer, Font *font = static_cast<Font *>(&fontFace0))
        -> int;
    inline auto getTextWidthQuick(const char *buffer, Font *font = static_cast<Font *>(&fontFace0))
        -> int {
        if (font == nullptr) {
            return -1;
        } else {
            return font->getTextWidthQuick(buffer);
        }
    }

    // MARK: - Graphic drawing code

    // Basically Adafruit_GFX::drawRect with the necessary partial refresh stuff
    // TODO: We might be able to instead just subclass `drawRect` here instead to be more consistent
    // with Adafruit_GFX
    void drawBox(int startX, int startY, int endX, int endY);

    // Draw a dotted box
    void drawDottedBox(int startX, int startY, int endX, int endY, int dotSize = 1,
                       int gapSize = 1);

    // Draw a bitmap
    void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                     uint16_t color);

    // Draw a bitmap scaled by a multiplier
    void drawScaledXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                           uint16_t color, int scale);

    void drawPBM(int16_t x, int16_t y, const std::string &fname, FileSystem &fs);
    void drawPBM(int16_t x, int16_t y, const uint8_t *pbmBytes, int size);
    void drawPBM(int16_t x, int16_t y, RendererStream &stream);

    // override for subclass, need access to raw drawing buffer
    virtual void drawFullscreenBitmapFromFile(RendererStream &stream) { log_i("not supported"); }

    // Clear the whole screen
    void clearScreen();

    // TODO: Just use `textcolor` and `textbgcolor` for these?
    uint16_t bg = 0, fg = 0;

    [[nodiscard]] virtual auto darkPictureVComAdj(uint8_t darkPixelsPercent) const -> int {
        return 0;
    }

#if SIMULATOR
    // Signal to the simulator to draw the screen
    virtual auto renderQueue() -> QueueHandle_t = 0;

    // The buffer that the simulator will use to draw the screen
    // For EInk simulation, this indicates the position of the pigment particles in the screen
    virtual auto buffer() -> std::unique_ptr<uint8_t[]> & = 0;

    // For the simulator to know if it should skip the waveform processing
    virtual auto getSkipWaveform() -> bool { return false; }
    virtual void setSkipWaveform(bool skip) {}

    // For the simulator to know if it should skip the signalling queue
    virtual auto getSkipSignalling() -> bool { return false; }
    virtual void setSkipSignalling(bool skip) {}
#endif

protected:
    [[nodiscard]] virtual auto nativeWidth() const -> uint16_t = 0;
    [[nodiscard]] virtual auto nativeHeight() const -> uint16_t = 0;

    // Constant screen inset. These can be overridden to permanently ignore part of the screen area.
    [[nodiscard]] virtual auto displayInsets() const -> DisplayInset {
        return DisplayInset::zero();
    }

    [[nodiscard]] inline auto xOffset(int16_t value) const -> int16_t {
        return value + displayInsets().left;
    }
    [[nodiscard]] inline auto yOffset(int16_t value) const -> int16_t {
        return value + displayInsets().top;
    }

    void rotate(uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h);

    void computeMinMax();

    [[nodiscard]] virtual auto getBuffer() -> uint8_t * = 0;

    // Must be set in the constructor of subclasses
    Adafruit_GFX *gfxClass_ = nullptr;

    int partialRefresh_ = 0;
    uint16_t partialX_ = 0, partialY_ = 0, partialW_ = 0, partialH_ = 0;

    bool invertedDisplay_ = false;

    bool forceRefreshType_ = false;
};

#endif
