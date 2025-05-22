#pragma once

#include "Communications/SindarinAPI.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Dispatch.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class KindleLastReadDialogScreen : public EinkView, public Screen {
public:
    KindleLastReadDialogScreen(DisplaySystem &display, const SpiramString &dialogText)
        : EinkView(display), dialogBoxBackground_(display, 5), dialogBoxForeground_(display, 5),
          dialogTextView_(display, 5), menuView_(display) {

        // Set up the dialog text
        dialogTextView_.setText(std::string(dialogText.c_str()));
        dialogTextView_.alignCenter = true;

        // Set up menu items for the two options
        menuView_.addMenuItem(std::make_shared<MenuItemSingleLine>(
            "Yes, go to latest page", [this]() { handleResponse(true); }));

        menuView_.addMenuItem(std::make_shared<MenuItemSingleLine>(
            "No, stay on current page", [this]() { handleResponse(false); }));

        menuView_.padding = 0;
        menuView_.itemsPerPage = 2;
        menuView_.setIndicatorType(MenuView::IndicatorType::Dot);

        menuView_.setSelectedIndex(0);

        setFirstLoad();
    }

    ~KindleLastReadDialogScreen() override = default;

    void render(DisplaySystem &display) override {
        setFrame(defaultFrameRect());
        layoutIfNeeded();

        draw();

        if (firstLoad_) {
            firstLoad_ = false;
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true);
            draw();
        }

        menuView_.drawFastElements();
        display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
    }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        auto contentFrame = context.setSublayoutFrame(
            SolRect::fromSize(size), LayoutOptionsSizeToFit | LayoutOptionsAlignCenter,
            [this](LayoutContext &context, SolSize size) -> SolSize {
                int y = 15;

                auto textBox =
                    context.setViewFrame(dialogTextView_, {{10, y}, {size.width - 20, 0}},
                                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop |
                                             LayoutOptionsAlignCenterHorizontal);

                y += textBox.size.height + 15;

                auto menuBox = context.setViewFrame(
                    menuView_, {{10, y}, {size.width - 20, size.height - y - 10}},
                    LayoutOptionsSizeToFit | LayoutOptionsAlignTop |
                        LayoutOptionsAlignCenterHorizontal);

                y += menuBox.size.height + 15;

                return {size.width * 5 / 6, y};
            });
        auto foregroundFrame =
            context.setViewFrame(dialogBoxForeground_, contentFrame.inset(-5, -5));
        auto backgroundFrame =
            context.setViewFrame(dialogBoxBackground_, foregroundFrame.inset(-3));

        return backgroundFrame.size;
    }

    void drawInRect(SolRect rect) override {
        dialogBoxBackground_.fillColor = display_.bg;
        dialogBoxBackground_.strokeColor = display_.bg;
        dialogBoxForeground_.strokeColor = display_.fg;

        dialogBoxBackground_.draw();
        dialogBoxForeground_.draw();
        dialogTextView_.draw();
        menuView_.draw();
    }

    void processCommand(RemoteCommand command) override {
        switch (command) {
        case RemoteCommand::UP:
            menuView_.moveSelection(-1);
            setNeedsRerender();
            break;
        case RemoteCommand::DOWN:
            menuView_.moveSelection(1);
            setNeedsRerender();
            break;
        case RemoteCommand::ENTER:
            menuView_.executeSelectedItem();
            break;
        default:
            break;
        }
    }

    void setResponseCallback(std::function<void(bool)> callback) {
        responseCallback_ = std::move(callback);
    }

private:
    void handleResponse(bool gotoLastReadPage) {
        // Call the callback immediately with the user's choice
        // The parent class (KindleReadableScreen) will handle the API call
        if (responseCallback_) {
            responseCallback_(gotoLastReadPage);
        }
    }

    // UI Components
    BoxView dialogBoxBackground_;
    BoxView dialogBoxForeground_;
    MultiLineTextView dialogTextView_;
    MenuView menuView_;

    // Callback for when user makes a selection
    std::function<void(bool)> responseCallback_;
};
