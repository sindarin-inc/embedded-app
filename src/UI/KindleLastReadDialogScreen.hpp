#pragma once

#include "UI/Components/EinkView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Screen.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Dispatch.hpp"

class KindleLastReadDialogScreen : public EinkView, public Screen {
public:
    KindleLastReadDialogScreen(DisplaySystem &display, const std::string &dialogText)
        : EinkView(display), 
          dialogBoxBackground_(display, 5),
          dialogBoxForeground_(display, 5),
          dialogTextView_(display, 5),
          menuView_(display) {
        
        // Set up the dialog text
        dialogTextView_.setText(dialogText);
        dialogTextView_.alignCenter = true;
        
        // Set up menu items for the two options
        auto menuItems = std::make_shared<std::vector<std::shared_ptr<MenuItem>>>();
        
        // "Yes, sync to latest" option
        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
            "Yes, sync to latest", [this]() { handleResponse(true); }));
        
        // "No, stay on current page" option
        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
            "No, stay on current page", [this]() { handleResponse(false); }));
        
        // Set up menu view
        // Add menu items to the view
        for (const auto& item : *menuItems) {
            menuView_.addMenuItem(item);
        }
        
        menuView_.padding = 0;
        menuView_.itemsPerPage = 2;
        menuView_.setIndicatorType(MenuView::IndicatorType::Dot);
        
        // Set first option as default (but allowing user to navigate)
        menuView_.setSelectedIndex(0);
        
        // Set first load flag to true
        setFirstLoad();
    }
    
    ~KindleLastReadDialogScreen() override = default;
    
    void render(DisplaySystem &display) override {
        setFrame(defaultFrameRect());
        layoutIfNeeded();
        
        // Draw the dialog components
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
                int y = 20; // Start with some padding at the top
                
                // Layout dialog text
                auto textBox = context.setViewFrame(
                    dialogTextView_, {{20, y}, {size.width - 40, 0}},
                    LayoutOptionsSizeToFit | LayoutOptionsAlignTop | 
                    LayoutOptionsAlignCenterHorizontal);
                
                y += textBox.size.height + 20; // Add spacing after the text
                
                // Layout menu items
                auto menuBox = context.setViewFrame(
                    menuView_, {{20, y}, {size.width - 40, size.height - y - 20}},
                    LayoutOptionsSizeToFit | LayoutOptionsAlignTop | 
                    LayoutOptionsAlignCenterHorizontal);
                
                y += menuBox.size.height + 20; // Add padding at the bottom
                
                // Return the size needed for the content
                return {size.width * 3 / 4, y};
            });
        
        // Create frames for the box backgrounds
        auto foregroundFrame = context.setViewFrame(dialogBoxForeground_, contentFrame.inset(-10, -10));
        auto backgroundFrame = context.setViewFrame(dialogBoxBackground_, foregroundFrame.inset(-5));
        
        return backgroundFrame.size;
    }
    
    void drawInRect(SolRect rect) override {
        // Set colors for the dialog box
        dialogBoxBackground_.fillColor = display_.bg;
        dialogBoxBackground_.strokeColor = display_.bg;
        dialogBoxForeground_.strokeColor = display_.fg;
        
        // Draw the components
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