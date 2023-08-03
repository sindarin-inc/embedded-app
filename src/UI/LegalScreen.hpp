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
          baclLine1View_(display, "BACL: LISTED UL 62368-1,", &ibmf75Face0),
          baclLine2View_(display, "CAN/CSA C22,2 NO. 62368-1, A0049", &ibmf75Face0),
          fcc1LineView_(display, "FCC ID: 2BAHC-READER1,", &ibmf75Face0),
          fcc1LineViewPart2_(display, "IC: 30424-READER1, Model: ED1-READER", &ibmf75Face0),
          fcc2LineView_(display, "FCC ID: 2BAHC-PT1,", &ibmf75Face0),
          fcc2LineViewPart2_(display, "IC: 30424-PT1, Model: ED1-PT1", &ibmf75Face0),
          opTemp1_(display, "Operating ambient temperature:", &ibmf75Face0),
          opTemp2_(display, "45-100°F (7°C to 37°C)", &ibmf75Face0),
          nonOpTemp1_(display, "Nonoperating temperature:", &ibmf75Face0),
          nonOpTemp2_(display, "-4-113°F (-20°C to 45°C)", &ibmf75Face0),
          opAlt_(display, "Operating altitude: < 4000 meters", &ibmf75Face0),
          relativeHumidity1_(display, "Relative humidity:", &ibmf75Face0),
          relativeHumidity2_(display, "5\% to 95\% noncondensing", &ibmf75Face0),
          leftRightView_(display, "Press  \ue001 or \ue002  to see more", &ibmf75Face0),
          returnToMenuView_(display, "Press  \ue000  to return to System Settings", &ibmf75Face0){};

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    PowerManager &power_;

    IconView fccView_;
    IconView ceView_;
    IconView baclView_;

    bool logosPage_ = true;

    SingleLineTextView baclLine1View_;
    SingleLineTextView baclLine2View_;
    SingleLineTextView fcc1LineView_;
    SingleLineTextView fcc1LineViewPart2_;
    SingleLineTextView fcc2LineView_;
    SingleLineTextView fcc2LineViewPart2_;

    SingleLineTextView opTemp1_;
    SingleLineTextView opTemp2_;
    SingleLineTextView nonOpTemp1_;
    SingleLineTextView nonOpTemp2_;
    SingleLineTextView opAlt_;
    SingleLineTextView relativeHumidity1_;
    SingleLineTextView relativeHumidity2_;
    SingleLineTextView leftRightView_;
    SingleLineTextView returnToMenuView_;
};

#endif