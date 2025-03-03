#pragma once

#include "Communications/WiFi.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"

class WiFiScanScreen : public Screen, public EinkView {
public:
    WiFiScanScreen(DisplaySystem &display, WiFi &wifi)
        : EinkView(display), display_(display), wifi_(wifi), statusView_(display, "Scanning..."){};

    void begin();
    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

private:
    DisplaySystem &display_;
    WiFi &wifi_;
    SingleLineTextView statusView_;
    std::vector<SingleLineTextView> networkViews_;
};
