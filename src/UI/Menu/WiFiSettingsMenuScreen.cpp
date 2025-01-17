#include "WiFiSettingsMenuScreen.hpp"

#include "Communications/WiFiSettings.hpp"
#include "Misc/Platform.hpp"

WiFiSettingsMenuScreen::WiFiSettingsMenuScreen(DisplaySystem &display)
    : EinkView(display), menuView_(display), backLabelView_(display, "System Settings"),
      configuredAccessView_(display, "Configured Access Points:"),
      configuredAccessPointsView_(display, MAX_WIFI_SETTINGS) {
    menuView_.padding = 1;
    loadWifiSettings();
}

void WiFiSettingsMenuScreen::loadWifiSettings() {
    WiFiSettings settings;
    settings.load();
    if (settings.count() > 0) {
        std::string listOfAccessPoints;
        configuredAccessView_.setText("Configured Access Points:");
        for (int i = 0; i < settings.count(); i++) {
            listOfAccessPoints += settings.settings[i].ssid + "\n";
        }
        configuredAccessPointsView_.setText(listOfAccessPoints);
    } else {
        configuredAccessView_.setText("No configured Access Points");
        configuredAccessPointsView_.setText("");
    }
    setNeedsLayout();
}

void WiFiSettingsMenuScreen::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void WiFiSettingsMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
        menuView_.executeSelectedItem();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        firstLoad_ = true;
        setNeedsRerender();
        back();
        return;
    default:
        break;
    }
    setNeedsLayout();
}

auto WiFiSettingsMenuScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
    auto menuBox = context.setViewFrame(
        menuView_, SolRect::fromSize(size).inset(size.width / 8, size.height / 6),
        LayoutOptionsAlignCenterHorizontal | LayoutOptionsSizeToFit);

    auto configuredLabelBox = context.setViewFrame(
        configuredAccessView_,
        {{15, menuBox.bottom() + 15}, {size.width - 15, size.height - menuBox.bottom() - 15}},
        LayoutOptionsSizeToFitVertical);

    WiFiSettings settings;

    int y = configuredLabelBox.bottom() + 2;

    context.setViewFrame(configuredAccessPointsView_,
                         {{size.width / 8, y}, {size.width - size.width / 8, size.height - y}},
                         LayoutOptionsSizeToFit);

    return size;
}

void WiFiSettingsMenuScreen::drawInRect(SolRect rect) {
    backLabelView_.draw();
    menuView_.draw();
    configuredAccessView_.draw();
    configuredAccessPointsView_.draw();
}

void WiFiSettingsMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

void WiFiSettingsMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
