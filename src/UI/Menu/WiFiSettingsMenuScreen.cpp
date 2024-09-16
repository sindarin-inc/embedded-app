#include "WiFiSettingsMenuScreen.hpp"

#include "Communications/WiFiSettings.hpp"
#include "Misc/Platform.hpp"
#include "UI/MsgPopup.hpp"

WiFiSettingsMenuScreen::WiFiSettingsMenuScreen(DisplaySystem &display, PreferencesStore &prefs)
    : EinkView(display), prefs_(prefs), menuView_(display),
      backLabelView_(display, "System Settings"), wifiEnabledView_(display, ""),
      bleEnabledView_(display, ""), configuredAccessView_(display, "Configured Access Points:    "),
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
    }
    setNeedsLayout();
}

void WiFiSettingsMenuScreen::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
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
        back();
        setNeedsRerender();
        break;
    default:
        break;
    }
    setNeedsLayout();
}

auto WiFiSettingsMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int y = size.height / 7;
    context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    std::string newWifiText = "WiFi ";

    if (prefs_.wifiDisabled.get().value_or(false)) {
        newWifiText += "Disabled";
    } else {
        newWifiText += "Enabled";
    }
    wifiEnabledView_.setText(newWifiText);

    std::string newBleText = "Bluetooth Sync ";
    if (prefs_.bleDisabled.get().value_or(false)) {
        newBleText += "Disabled";
    } else {
        newBleText += "Enabled";
    }
    bleEnabledView_.setText(newBleText);

    auto wifiEnabledBox =
        context.setViewFrame(wifiEnabledView_, {{0, y}, {size.width, 0}},
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);

    y += wifiEnabledBox.size.height + 1;

    context.setViewFrame(bleEnabledView_, {{0, y}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);

    auto menuBox =
        context.setViewFrame(menuView_, Rect::fromSize(size).inset(0, size.height / 3),
                             LayoutOptionsAlignCenterHorizontal | LayoutOptionsSizeToFit);

    auto configuredLabelBox = context.setViewFrame(
        configuredAccessView_,
        {{15, menuBox.bottom() + 15}, {size.width - 15, size.height - menuBox.bottom() - 15}},
        LayoutOptionsSizeToFit);

    WiFiSettings settings;

    y = configuredLabelBox.bottom() + 2;

    context.setViewFrame(configuredAccessPointsView_, {{18, y}, {size.width - 18, size.height - y}},
                         LayoutOptionsSizeToFit);

    return size;
}

void WiFiSettingsMenuScreen::showTemporaryMessage(DisplaySystem &display,
                                                  const std::string &message, int duration,
                                                  bool rerenderAfter) {
    auto *popup = new MsgPopup(display, message);
    popup->render(display);
    delete popup;
    if (rerenderAfter) {
        Delay(duration);
        setNeedsRerender();
        this->render(display);
    }
}

void WiFiSettingsMenuScreen::drawInRect(Rect rect) {
    backLabelView_.draw();
    wifiEnabledView_.draw();
    bleEnabledView_.draw();
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

    menuView_.drawIndicator();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
