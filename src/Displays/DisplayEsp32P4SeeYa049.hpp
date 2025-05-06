#pragma once

#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>

#include "DisplaySystem.hpp"

class DisplayEsp32P4SeeYa049 : public DisplaySystem {
public:
    DisplayEsp32P4SeeYa049() noexcept : gfxCanvas_(CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT) {
        gfxClass_ = &gfxCanvas_;
    };

    DisplayEsp32P4SeeYa049(DisplayEsp32P4SeeYa049 const &) = delete;

    ~DisplayEsp32P4SeeYa049() override {
        if (updateSemaphore_ != nullptr) {
            vSemaphoreDelete(updateSemaphore_);
        }
    };

    [[nodiscard]] auto defaultTextInsets() const -> DisplayInset override {
        return {20, 20, 10, 10};
    };
    [[nodiscard]] auto minTextInsets() const -> DisplayInset override { return {5, 5, 5, 5}; };
    [[nodiscard]] auto minTextBoxWidth() const -> int16_t override { return 125; };
    [[nodiscard]] auto minTextBoxHeight() const -> int16_t override { return 45; };

    auto setup() -> bool override;

    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       SolRect altRect = {0, 0, 0, 0}) override;

protected:
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return CONFIG_DISPLAY_WIDTH; }
    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return CONFIG_DISPLAY_HEIGHT; }
    [[nodiscard]] auto getBuffer() -> uint8_t * override {
        return reinterpret_cast<uint8_t *>(gfxCanvas_.getBuffer());
    }

    IRAM_ATTR static auto onColorTransDone(esp_lcd_panel_handle_t panel,
                                           esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
        -> bool;

private:
    GFXcanvas24 gfxCanvas_;
    esp_ldo_channel_handle_t ldoMipiPhy_ = nullptr;
    esp_lcd_dsi_bus_handle_t mipiDsiBus_ = nullptr;
    esp_lcd_panel_handle_t panelHandle_ = nullptr;
    esp_lcd_panel_io_handle_t mipiDbiIo_ = nullptr;

    SemaphoreHandle_t updateSemaphore_ = nullptr;
};
