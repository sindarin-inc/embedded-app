#ifndef LEGAL_SCREEN_HPP
#define LEGAL_SCREEN_HPP

#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Bitmaps/bacl.hpp"
#include "UI/Bitmaps/ce.hpp"
#include "UI/Bitmaps/fcc.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"
#include "Version.hpp"

class LegalScreen : public Screen, public EinkView {
public:
    LegalScreen(DisplaySystem &display, PreferencesStore &prefs, PowerManager &power)
        : EinkView(display), display_(display), prefs_(prefs), power_(power),
          fccView_(display, FCC_BITS, fcc_width, fcc_height),
          ceView_(display, CE_BITS, ce_width, ce_height),
          baclView_(display, BACL_BITS, bacl_width, bacl_height),
          productNameView_(display, "BACL: LISTED UL 62368-1,", &ibmf75Face0),
          serialNumberView_(display, "CAN/CSA C22,2 NO. 62368-1, A0049", &ibmf75Face0),
          firmwareVersionView_(display, "", &ibmf75Face0),
          remoteFirmwareVersionView_(display, "FCC IDs: 2BAHC-READER1, 2BAHC-PT1", &ibmf75Face0),
          remoteBatteryView_(display, "IC: 30424", &ibmf75Face0),
          returnToMenuView_(display, "Press  \ue000  to return to System Settings", &ibmf75Face0) {
        //  productNameView_.setText("Product Name: Sol Reader");

        // serialNumberView_.setText(serialStr);

        //        firmwareVersionView_.setText(buildString);

        //      remoteFirmwareVersionView_.setText(ptBattery);
        //        remoteBatteryView_.setText(ptBuildString);

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

    IconView fccView_;
    IconView ceView_;
    IconView baclView_;
    SingleLineTextView productNameView_;
    SingleLineTextView serialNumberView_;
    SingleLineTextView firmwareVersionView_;
    SingleLineTextView remoteFirmwareVersionView_;
    SingleLineTextView remoteBatteryView_;
    SingleLineTextView returnToMenuView_;
};

#endif