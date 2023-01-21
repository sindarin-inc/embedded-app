#pragma once

#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"

class LightingAdjustScreen : public Screen, public EinkView {
public:
    LightingAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                         LightingController &lightingController);

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    bool firstLoad = true;

protected:
    PreferencesStore &prefs_;
    LightingController &lightingController_;
    bool showThirdLine_;

    // Subviews
    IconView lightbulbView_;
    BoxView barView_;
    BoxView fillView_;
    // TODO; These first two lines could be one view once we have MultiLineTextView
    SingleLineTextView firstLine_;
    SingleLineTextView secondLine_;
    SingleLineTextView thirdLine_;
    ArrowView arrowView_;
};
