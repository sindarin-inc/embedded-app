#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <memory>
#include <utility>
#include <vector>

#include "UI/Bitmaps/DownloadAppQr.xbm"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"

// Example of a more complex view that combines other views in a layout
class AppDownloadScreen : public Screen, public EinkView {
public:
    AppDownloadScreen(DisplaySystem &display,
                      std::function<void()> makeAndStartContentSync = nullptr)
        : Screen(), EinkView(display),
          syncPollingTimer_(xTimerCreate("syncPollingTimer", pdMS_TO_TICKS(2000), pdTRUE, this,
                                         AppDownloadScreen::timerCallback)),
          topLineView_(display, 3),
          iconView_(display, DOWNLOAD_APP_QR_BITS, DownloadAppQr_width, DownloadAppQr_height, 3),
          bottomLine_(display, "solreader.com/app"),
          makeAndStartContentSync_(std::move(makeAndStartContentSync)) {
        topLineView_.setText(
            "Purchase books, connect to your Sol Reader, set up WiFi, read, and more "
            "with the Sol app");
        topLineView_.alignCenter = true;

        xTimerStart(syncPollingTimer_, 0);
    };
    ~AppDownloadScreen() override {
        xTimerStop(syncPollingTimer_, 0);
        xTimerDelete(syncPollingTimer_, 0);
    }

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

    static void timerCallback(TimerHandle_t timer);

protected:
    // TODO: It'd be nicer to have his be event driven, but seems overly heavy to set up a global
    // event group just for this one state.
    TimerHandle_t syncPollingTimer_;

    MultiLineTextView topLineView_;
    IconView iconView_;
    SingleLineTextView bottomLine_;

    std::function<void()> makeAndStartContentSync_;
};
