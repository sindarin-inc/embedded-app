#pragma once

#include <memory>
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
    AppDownloadScreen(DisplaySystem &display)
        : Screen(), EinkView(display), topLineView_(display, 2),
          iconView_(display, DOWNLOAD_APP_QR_BITS, DownloadAppQr_width, DownloadAppQr_height, 3),
          bottomLine_(display, "solreader.com/app") {
        topLineView_.setText("Set up WiFi with the\nSol Reader app");
        topLineView_.alignCenter = true;
    };

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

protected:
    MultiLineTextView topLineView_;
    IconView iconView_;
    SingleLineTextView bottomLine_;
};
