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
#include "Misc/Style.hpp"
#include "Storage/FileSystem.hpp"
#include "UI/Fonts/Font.hpp"
#include "UI/Layout/Types.hpp"

#define INVERTED_DISPLAY true
// #define INVERTED_DISPLAY false

struct DisplayInset {
    uint16_t top;
    uint16_t bottom;
    uint16_t left;
    uint16_t right;

    [[nodiscard]] auto toString() const -> std::string {
        return StringFormat("t:%d, b:%d, l:%d, r:%d", top, bottom, left, right);
    }
};

class DisplaySystem {
public:
    DisplaySystem();
    DisplaySystem(DisplaySystem const &) = delete;
    virtual ~DisplaySystem() = default;

    virtual auto setup() -> bool;

    static void animationTimerCallback(TimerHandle_t timer);

    static TimerHandle_t animationTimer;

    Style style;

    // MARK: - Required virtual methods that subclasses must implement
    enum UpdateType {
        UNSPECIFIED,
        FULL,
        PARTIAL,
        FAST,
        FAST_WITH_VCOM_NUDGE,
        FAST_ORIGINAL_AU4,
        ORIGINAL_PARTIAL,
        ERASE,
    };
    virtual void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                               bool clearFrameBuffer = true, int vcomNudge = 0,
                               DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                               ScreenSide screen = ScreenSide::Both,
                               Rect altRect = {{0, 0}, {0, 0}}) = 0;

    virtual void beginPowerOn() {}

    // the insets must be set in the constructor of the subclass
    inline virtual auto getDisplayInsets() -> DisplayInset { return insets_; };
    virtual auto getDefaultInsets() -> DisplayInset = 0;
    virtual auto getDisplayWidth() -> uint16_t = 0;
    virtual auto getDisplayHeight() -> uint16_t = 0;
    virtual auto getMinInsets() -> DisplayInset = 0;
    virtual void storeInsets() = 0;
    virtual void setTemporaryInsets(DisplayInset newInsets) = 0;

    // Use `gfxClass` when you need to get direct access to draw to
    // the whole screen (potentially outside of focus areas)
    [[nodiscard]] auto gfxClass() const -> Adafruit_GFX * { return gfxClass_; }

    // MARK: - Brought in from Adafruit_GFX
    // Use these methods for normal drawing as they'll take into account the offsets and focus area
    // automatically. Any methods we need from Adafruit_GFX that need to respect the focus area
    // should be copied in here.
    [[nodiscard]] auto width() -> int16_t {
        return (getDisplayWidth() - getDisplayInsets().left - getDisplayInsets().right);
    };

    [[nodiscard]] auto height() -> int16_t {
        return (getDisplayHeight() - getDisplayInsets().top - getDisplayInsets().bottom);
    };

    void fillScreen(uint16_t color) { gfxClass_->fillScreen(color); }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      uint16_t color) {
        gfxClass_->fillTriangle(xOffset(x0), yOffset(y0), xOffset(x1), yOffset(y1), xOffset(x2),
                                yOffset(y2), color);
    }
    void setFont(Font *f) {
        if (f != nullptr) {
            font_ = f;
            auto gFont = dynamic_cast<GFont *>(font_);
            if (gFont != nullptr) {
                gfxClass_->setFont(gFont->get());
            }
        } else {
            log_e("Received a bad (null) font pointer!");
        }
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

    [[nodiscard]] auto getLineSpacing() const -> int { return lineSpacing_; }
    void setLineSpacing(int newSpacing) { lineSpacing_ = newSpacing; }

    virtual void clearDisplay(){};
    virtual void clearFrameBuffer() { fillScreen(bg); };

    void forceDontChangeRefreshType() { forceRefreshType_ = true; }
    void clearDontChangeRefreshType() { forceRefreshType_ = false; }

    // MARK: - Text rendering code
    auto getFont() -> Font *;
    void drawSingleLineOfText(const std::string &line, int x, int y, bool inverted = false);
    void drawSingleLineOfCenteredText(const std::string &line);
    void getTextSize(const std::string &buffer, uint16_t *w, uint16_t *h);
    auto getTextHeight() -> int;
    auto getTextHeight(const std::string &buffer) -> int;
    auto getTextWidth(const std::string &buffer) -> int;

    inline auto getTextWidthQuick(const char *buffer) -> int {
        return dynamic_cast<IBMFFont *>(font_)->getTextWidthQuick(buffer);
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

    // When we want to ignore the configured offsets
    [[nodiscard]] inline auto xAbsolute(int x) -> int { return x - getDisplayInsets().left; }
    [[nodiscard]] inline auto yAbsolute(int y) -> int { return y - getDisplayInsets().top; }

    // Should these be in the subclasses instead?
    [[nodiscard]] auto xAbsoluteCenter() -> int { return getDisplayWidth() / 2; }
    [[nodiscard]] auto yAbsoluteCenter() -> int { return getDisplayHeight() / 2; }

    [[nodiscard]] auto xAbsoluteMax() -> int { return getDisplayWidth() - 1; }
    [[nodiscard]] auto yAbsoluteMax() -> int { return getDisplayHeight() - 1; }

    [[nodiscard]] auto absoluteWidth() -> int { return getDisplayWidth(); }
    [[nodiscard]] auto absoluteHeight() -> int { return getDisplayHeight(); }

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
    // Must be set in the constructor of subclasses
    Adafruit_GFX *gfxClass_ = nullptr;
    Font *font_ = nullptr;
    uint8_t *canvasBuffer_{};
    uint16_t canvasWidth_{};
    uint16_t canvasHeight_{};

    [[nodiscard]] inline auto xOffset(int16_t value) -> int16_t {
        return value + getDisplayInsets().left;
    }
    [[nodiscard]] inline auto yOffset(int16_t value) -> int16_t {
        return value + getDisplayInsets().top;
    }

    void rotate(uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h);
    void computeMinMax();

    int partialRefresh_ = 0;
    uint16_t partialX_ = 0, partialY_ = 0, partialW_ = 0, partialH_ = 0;

    bool invertedDisplay_ = false;

    bool forceRefreshType_ = false;

    int lineSpacing_ = 0;

    DisplayInset insets_ = {0, 0, 0, 0};
};

#endif
