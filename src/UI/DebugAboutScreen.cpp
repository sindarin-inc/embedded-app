#include "UI/DebugAboutScreen.hpp"

#include <esp_app_desc.h>
#include <hal/efuse_hal.h>

#include "sindarin-debug.h"

DebugAboutScreen::DebugAboutScreen(DisplaySystem &display)
    : EinkView(display), display_(display),
      returnToMenuView_(display, "Press  \ue000  to return to Debug Settings") {

    auto serialStr = StringFormat("SN: %s", GetSerialNumber().c_str());
    textViews_.emplace_back(std::make_shared<SingleLineTextView>(display, serialStr));

    const esp_app_desc_t *appDesc = esp_app_get_description();

    auto versionStr = StringFormat("Version: %s", appDesc->version);
    textViews_.emplace_back(std::make_shared<SingleLineTextView>(display, versionStr));

    auto compileTimeStr = StringFormat("Compile Time: %s %s", appDesc->date, appDesc->time);
    textViews_.emplace_back(std::make_shared<SingleLineTextView>(display, compileTimeStr));

    auto idfVersionStr = StringFormat("IDF Version: %s", appDesc->idf_ver);
    textViews_.emplace_back(std::make_shared<SingleLineTextView>(display, idfVersionStr));

    char buf[10];
    esp_app_get_elf_sha256(buf, sizeof(buf));
    auto elfSha256Str = StringFormat("ELF SHA256: %s...", buf);
    textViews_.emplace_back(std::make_shared<SingleLineTextView>(display, elfSha256Str));

    unsigned revision = efuse_hal_chip_revision();
    auto chipRevStr = StringFormat("Chip rev: v%d.%d", revision / 100, revision % 100);
    textViews_.emplace_back(std::make_shared<SingleLineTextView>(display, chipRevStr));
}

auto DebugAboutScreen::layout(LayoutContext &context, Size size) -> Size {
    uint8_t yAdvance = 17;

    int x = 0;
    int y = 10;

    for (auto &view : textViews_) {
        context.setViewFrame(*view, {{x, y}, size},
                             LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
        y += yAdvance;
    }

    context.setViewFrame(returnToMenuView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return size;
}

void DebugAboutScreen::drawInRect(Rect rect) {
    for (auto &view : textViews_) {
        view->draw();
    }
    returnToMenuView_.draw();
}

void DebugAboutScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    int adjustedXPadding = display.defaultXPadding - 5;
    int adjustedYPadding = display.defaultYPadding - 5;
    setFrame({{display.xAbsolute(adjustedXPadding), display.yAbsolute(adjustedYPadding)},
              {display.defaultScreenWidth - 2 * adjustedXPadding,
               display.defaultScreenHeight - 2 * adjustedYPadding}});

    draw();
    display.updateDisplay();
}

void DebugAboutScreen::processCommand(RemoteCommand command) {
    if (command == RemoteCommand::ENTER) {
        back();
    }
}
