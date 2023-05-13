#ifndef ABOUT_SCREEN_HPP
#define ABOUT_SCREEN_HPP

#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Bitmaps/im100.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"
#include "Version.hpp"

class AboutScreen : public Screen, public EinkView {
public:
    AboutScreen(DisplaySystem &display, PreferencesStore &prefs, PowerManager &power)
        : EinkView(display), display_(display), prefs_(prefs), power_(power),
          solLogoView_(display, IM100_BITS, im100_width, im100_height),
          productNameView_(display, "Product Name: Sol Reader", &ibmf75Face0),
          serialNumberView_(display, "", &ibmf75Face0),
          firmwareVersionView_(display, "", &ibmf75Face0),
          remoteFirmwareVersionView_(display, "", &ibmf75Face0),
          remoteBatteryView_(display, "", &ibmf75Face0),
          returnToMenuView_(display, "Press  \ue000  to return to System Settings", &ibmf75Face0) {
        productNameView_.setText("Product Name: Sol Reader");

        auto serialStr = StringFormat("Serial Number: %s", GetSerialNumber().c_str());
        serialNumberView_.setText(serialStr);

#ifdef GITHASH
        std::string buildString = StringFormat("Reader Firmware:  %s", kVersionString);
#else
        std::string buildString = StringFormat("Reader Firmware: %s", kVersionString);
#endif

        firmwareVersionView_.setText(buildString);

        auto ptBatteryPct = PreferencesStore::lastPageTurnerBattery;
        auto ptBuild = prefs_.lastPageTurnerBuild.get().value_or("Unknown build");
        auto ptBattery = StringFormat("Remote Firmware: %s", ptBuild.c_str());
        auto ptBuildString = StringFormat("Remote Battery: %d%%", ptBatteryPct);

        remoteFirmwareVersionView_.setText(ptBattery);
        remoteBatteryView_.setText(ptBuildString);

        secretCodeCommands_.push_back(RemoteCommand::PREV);
        secretCodeCommands_.push_back(RemoteCommand::PREV);
        secretCodeCommands_.push_back(RemoteCommand::NEXT);
        secretCodeCommands_.push_back(RemoteCommand::NEXT);
        secretCodeCommands_.push_back(RemoteCommand::CHANNEL_MINUS);
        secretCodeCommands_.push_back(RemoteCommand::CHANNEL_PLUS);
        secretCodeCommands_.push_back(RemoteCommand::CHANNEL_MINUS);
        secretCodeCommands_.push_back(RemoteCommand::CHANNEL_PLUS);
    };

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    PowerManager &power_;

    std::vector<RemoteCommand> secretCodeCommands_;
    uint8_t secretCodeIndex_ = 0;

    IconView solLogoView_;
    SingleLineTextView productNameView_;
    SingleLineTextView serialNumberView_;
    SingleLineTextView firmwareVersionView_;
    SingleLineTextView remoteFirmwareVersionView_;
    SingleLineTextView remoteBatteryView_;
    SingleLineTextView returnToMenuView_;
};

#endif