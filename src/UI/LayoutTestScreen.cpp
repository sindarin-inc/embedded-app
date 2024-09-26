#include "LayoutTestScreen.hpp"

auto LayoutTestScreen::layout(LayoutContext &context, Size size) -> Size {
    // inset all around by 2px
    int x = 2;
    int y = 2;
    size.width = size.width - 4;

    // Draw the icon vertically aligned on the left of the screen
    auto iconRect = context.setViewFrame(iconView_, {{x, y}, size},
                                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft |
                                             LayoutOptionsAlignCenterVertical);

    // Create a sublayout to draw a bunch of stuff centered vertically on the remaining right
    // area of the screen. This is kinda like making a figma group of a bunch of layers then
    // center-aligning the whole group
    x = iconRect.origin.x + iconRect.size.width + 2;
    auto rightColumnRect = context.setSublayoutFrame(
        {{x, 0}, {size.width - x, size.height}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;

            // Now on the same top pixel as the logo, draw the top text
            int textWidth = size.width - x;
            auto topTextRect = context.setViewFrame(
                topView_, {{x, y}, {textWidth, size.height}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignTop);

            // Draw the next line immediately below but with a 2px offset
            y += topTextRect.size.height;
            auto bottomTextRect = context.setViewFrame(
                bottomView_, {{x, y}, {textWidth, size.height}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignTop);

            y += bottomTextRect.size.height;
            auto tripleBoxRect = context.setViewFrame(tripleBoxView_, {{x, y}, {textWidth, 10}});

            y += tripleBoxRect.size.height + 2;

            // Create a sublayout with some horizontal elements
            auto sublayoutRect = context.setSublayoutFrame(
                {{x, y}, {textWidth, 10}}, LayoutOptionsSizeToFit,
                [this](LayoutContext &context, Size size) -> Size {
                    int x = 0;
                    int y = 0;

                    // Text with a box around it
                    auto text1Rect = context.setViewFrame(text1_, {{x + 4, y + 2}, {20, 20}},
                                                          LayoutOptionsSizeToFit);

                    auto box1Rect = context.setViewFrame(
                        box1_, {{x, y}, {text1Rect.size.width + 8, text1Rect.size.height + 4}});
                    y += box1Rect.size.height + 2;

                    auto box2Rect = context.setViewSizeInRect(
                        box2_, {10, 10}, {{x, y}, {size.width - x, 10}}, LayoutOptionsAlignRight);
                    y += box2Rect.size.height;

                    return Size{size.width, y};
                });

            y += sublayoutRect.size.height + 2;
            auto bottomBoxRect = context.setViewFrame(bottomBoxView_, {{x, y}, {textWidth, 15}});

            y += bottomBoxRect.size.height + 2;
            auto evenSpacedWordsRect =
                context.setViewFrame(evenSpacedWordsView_, {{x, y}, {textWidth, 15}});
            y += evenSpacedWordsRect.size.height + 2;

            auto crazyBoxRect =
                context.setViewSizeInRect(crazyBoxView_, {size.width - 40, 15},
                                          {{x, y}, {size.width, 15}}, LayoutOptionsAlignRight);
            y += crazyBoxRect.size.height;

            return {size.width, y};
        });
    return {size.width, std::max(rightColumnRect.size.height, iconRect.size.height)};
};

void LayoutTestScreen::drawInRect(Rect rect) {
    iconView_.draw();
    topView_.draw();
    bottomView_.draw();
    tripleBoxView_.draw();
    bottomBoxView_.draw();
    evenSpacedWordsView_.draw();
    box1_.draw();
    box2_.draw();
    text1_.draw();
    crazyBoxView_.draw();
}

void LayoutTestScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    PROFILE_START(LayoutTestScreenLayoutAndDraw);
    draw();
    PROFILE_END_LOG_US(LayoutTestScreenLayoutAndDraw);

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST);
    firstLoad = false;
}

void LayoutTestScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
        back();
        break;
    default:
        break;
    }
}
