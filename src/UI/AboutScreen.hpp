#pragma once

#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/SecretCode.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Bitmaps/sol-logo.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "UI/Screen.hpp"
#include "Version.hpp"

class AboutScreen : public Screen, public EinkView {
public:
    AboutScreen(DisplaySystem &display, PreferencesStore &prefs, PowerManager &power)
        : EinkView(display), display_(display), prefs_(prefs), power_(power),
          solLogoView_(display, SOL_LOGO_BITS, sol_logo_width, sol_logo_height),
          productNameView_(display, "Product Name: Sol Reader", &fontFace0),
          serialNumberView_(display, "", &fontFace0), firmwareVersionView_(display, "", &fontFace0),
          wifiMacView_(display, "", &fontFace0),
          remoteFirmwareVersionView_(display, "", &fontFace0),
          remoteBatteryView_(display, "", &fontFace0), userEmailView_(display, "", &fontFace0),
          returnToMenuView_(display, "Press  \ue000  to return to System Settings", &fontFace0) {
        productNameView_.setText("Product Name: Sol Reader");

        auto serialStr = StringFormat("Serial Number: %s", GetSerialNumber().c_str());
        serialNumberView_.setText(serialStr);

#ifdef GITHASH
        std::string buildString = StringFormat("Reader Firmware:  %s", SemanticVersionString());
#else
        std::string buildString = StringFormat("Reader Firmware: %s", SemanticVersionString());
#endif

        auto wifiMac = StringFormat("WiFi MAC: %s", GetMacAddress().c_str());
        wifiMacView_.setText(wifiMac);

        firmwareVersionView_.setText(buildString);

        auto ptBatteryPct = PreferencesStore::lastPageTurnerBattery;
        auto ptBuild = prefs_.lastPageTurnerBuild.get().value_or("Unknown build");
        auto ptBattery = StringFormat("Remote Firmware: %s", ptBuild.c_str());
        auto ptBuildString = StringFormat("Remote Battery: %d%%", ptBatteryPct);

        remoteFirmwareVersionView_.setText(ptBattery);
        remoteBatteryView_.setText(ptBuildString);

        auto userEmail = prefs_.userEmail.get();
        if (userEmail.has_value() && !userEmail.value().empty()) {
            auto emailStr = StringFormat("User Email: %s", userEmail.value().c_str());
            userEmailView_.setText(emailStr);
        } else {
            userEmailView_.setText("User Email: Not set");
        }

        secretCode_.setCallback([this]() {
            log_d("Got the right secret code -- toggling Debug Screen and rebooting!");
            prefs_.showDebugMenu.set(!prefs_.showDebugMenu.get().value_or(false));
            power_.reboot();
        });
    };

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    PowerManager &power_;

    SecretCode secretCode_;

    IconView solLogoView_;
    SingleLineTextView productNameView_;
    SingleLineTextView serialNumberView_;
    SingleLineTextView firmwareVersionView_;
    SingleLineTextView wifiMacView_;
    SingleLineTextView remoteFirmwareVersionView_;
    SingleLineTextView remoteBatteryView_;
    SingleLineTextView userEmailView_;
    SingleLineTextView returnToMenuView_;
};
