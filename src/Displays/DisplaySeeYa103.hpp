#pragma once

#include <driver/i2c_master.h>
#include <driver/ppa.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>

#include "DisplaySystem.hpp"

class DisplaySeeYa103 : public DisplaySystem {
public:
    DisplaySeeYa103(int sdaLGpio, int sclLGpio, int sdaRGpio, int sclRGpio) noexcept
        : gfxCanvas_(CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT), sdaLGpio_(sdaLGpio),
          sclLGpio_(sclLGpio), sdaRGpio_(sdaRGpio), sclRGpio_(sclRGpio) {
        gfxClass_ = &gfxCanvas_;
    };

    DisplaySeeYa103(DisplaySeeYa103 const &) = delete;

    ~DisplaySeeYa103() override;

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
    void *fb0_;
    ppa_client_handle_t ppaSrmClient_ = nullptr;

    // I2C configuration
    int sdaLGpio_;
    int sclLGpio_;
    int sdaRGpio_;
    int sclRGpio_;

    SemaphoreHandle_t updateSemaphore_ = nullptr;

    auto executeI2C(bool left, std::function<esp_err_t(i2c_master_dev_handle_t)> command)
        -> esp_err_t;

    auto executeI2C(std::function<esp_err_t(i2c_master_dev_handle_t)> command) -> esp_err_t;
};
