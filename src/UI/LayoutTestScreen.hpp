#include <memory>
#include <vector>

#include "Displays/DisplaySystem.hpp"
#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"

class EinkView : public View {
public:
    EinkView(DisplaySystem &display) : View(), display_(display) {}

protected:
    DisplaySystem &display_;
};

// A view that draws a line of text
class TextView : public EinkView {
public:
    TextView(DisplaySystem &display, std::string text)
        : EinkView(display), text_(std::move(text)) {}

    auto layout(LayoutContext &context, Size size) -> Size override {
        uint16_t w, h;
        display_.getTextBounds(text_.c_str(), &w, &h);
        return {w, display_.getFont()->yAdvance};
    }

    void drawInRect(Rect rect) override {
        log_d("Draw TextView in rect %d %d %d %d", rect.origin.x, rect.origin.y, rect.size.width,
              rect.size.height);
        layoutIfNeeded();
        display_.drawSingleLineOfText(text_, rect.origin.x, rect.origin.y);
    }

protected:
    std::string text_;
};

// A view that draws an icon
class IconView : public EinkView {
public:
    IconView(DisplaySystem &display, unsigned char *iconBits, int width, int height)
        : EinkView(display), iconBits_(iconBits), width_(width), height_(height) {}

    auto layout(LayoutContext &context, Size size) -> Size override { return {width_, height_}; }

    void drawInRect(Rect rect) override {
        layoutIfNeeded();
        display_.drawXBitmap(rect.origin.x, rect.origin.y, iconBits_, width_, height_, display_.fg);
    }

protected:
    unsigned char *iconBits_;
    int width_;
    int height_;
};

// Absurdly simple view that just draws a box
class BoxView : public EinkView {
public:
    BoxView(DisplaySystem &display) : EinkView(display) {}
    ~BoxView() { log_d("BoxView destroyed"); };

    auto layout(LayoutContext &context, Size size) -> Size override { return size; }

    void drawInRect(Rect rect) override {
        log_d("Draw BoxView in rect %d %d %d %d", rect.origin.x, rect.origin.y, rect.size.width,
              rect.size.height);
        layoutIfNeeded();
        display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                          display_.fg);
    }
};

class HBox : public EinkView {
public:
    HBox(DisplaySystem &display, std::vector<std::shared_ptr<EinkView>> views, int spacing = 2)
        : EinkView(display), views_(views), spacing_(spacing) {}

    auto layout(LayoutContext &context, Size size) -> Size override {
        log_d("HBox layout size %d %d", size.width, size.height);

        // Assume equal sizes for now
        int viewCount = views_.size();
        int boxWidth = (size.width - (viewCount - 1) * spacing_) / viewCount;
        int x = 0;
        int y = 0;

        for (auto &view : views_) {
            log_d("HBox setting view frame %d %d %d %d", x, y, boxWidth, size.height);
            context.setViewFrame(*view, {{x, y}, {boxWidth, size.height}},
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
            x += boxWidth + spacing_;
        }

        return size;
    }

    void drawInRect(Rect rect) override {
        log_d("Draw HBox in rect %d %d %d %d", rect.origin.x, rect.origin.y, rect.size.width,
              rect.size.height);
        layoutIfNeeded();
        for (auto &view : views_) {
            (*view).draw();
        }
    }

protected:
    std::vector<std::shared_ptr<EinkView>> views_;
    int spacing_;
};

// Example of a more complex view that combines other views in a layout
class ExampleView : public EinkView {
public:
    ExampleView(DisplaySystem &display)
        : EinkView(display), iconView_(display, lightbulbBits, Lightbulb_width, Lightbulb_height),
          topView_(display, "First row of text"), bottomView_(display, "Second row"),
          nestedTextView_(std::make_shared<TextView>(display, "HI")),
          tripleBoxView_(display, {std::make_shared<BoxView>(display),
                                   std::make_shared<HBox>(
                                       display,
                                       (std::vector<std::shared_ptr<EinkView>>){
                                           std::make_shared<BoxView>(display), nestedTextView_})}),
          bottomBoxView_(display,
                         {std::make_shared<BoxView>(display), std::make_shared<BoxView>(display),
                          std::make_shared<TextView>(display, "HIII")}),
          evenSpacedWordsView_(display, {std::make_shared<TextView>(display, "its"),
                                         std::make_shared<TextView>(display, "me,"),
                                         std::make_shared<TextView>(display, "hi"),
                                         std::make_shared<TextView>(display, "i'm"),
                                         std::make_shared<TextView>(display, "the")}),
          box1_(display), box2_(display), text1_(display, "Left") {
        log_d("Boxes %p %p", &box1_, &box2_);
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        log_d("ExampleView layout size %d %d", size.width, size.height);
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
                log_d("Outer sublayout lambda size = %d %d", size.width, size.height);
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
                auto tripleBoxRect =
                    context.setViewFrame(tripleBoxView_, {{x, y}, {textWidth, 10}});

                y += tripleBoxRect.size.height + 2;

                // Create a sublayout with some horizontal elements
                log_d("_layoutContext.sizing_ = %d, context.sizing_ = %d", context.sizing_,
                      context.sizing_);
                auto sublayoutRect = context.setSublayoutFrame(
                    {{x, y}, {textWidth, 10}}, LayoutOptionsSizeToFit,
                    [this](LayoutContext &context, Size size) -> Size {
                        log_d("Inner sublayout lambda size = %d %d", size.width, size.height);
                        int x = 0;
                        int y = 0;

                        // Text with a box around it
                        auto text1Rect = context.setViewFrame(text1_, {{x + 4, y + 2}, {0, 0}},
                                                              LayoutOptionsSizeToFit);

                        auto box1Rect = context.setViewFrame(
                            box1_, {{x, y}, {text1Rect.size.width + 8, text1Rect.size.height + 4}});
                        y += box1Rect.size.height + 2;

                        auto box2Rect = context.setViewSizeInRect(box2_, {10, 10},
                                                                  {{x, y}, {size.width - x, 10}},
                                                                  LayoutOptionsAlignRight);
                        y += box2Rect.size.height;

                        log_d("Sublayout lambda out size = %d %d", size.width, y);

                        return Size{size.width, y};
                    });

                y += sublayoutRect.size.height + 2;
                auto bottomBoxRect =
                    context.setViewFrame(bottomBoxView_, {{x, y}, {textWidth, 15}});

                y += bottomBoxRect.size.height + 2;
                auto evenSpacedWordsRect =
                    context.setViewFrame(evenSpacedWordsView_, {{x, y}, {textWidth, 15}});
                y += evenSpacedWordsRect.size.height;

                return {size.width, y};
            });

        return {size.width, std::max(rightColumnRect.size.height, iconRect.size.height)};
    };

    void drawInRect(Rect rect) override {
        log_d("Draw ExampleView in rect %d %d %d %d", rect.origin.x, rect.origin.y, rect.size.width,
              rect.size.height);
        layoutIfNeeded();
        //  TODO: We probably need a way to pass through the global coordinate system
        //  so that more deeply nested views get drawn in the right place
        iconView_.draw();
        // This doesn't work for nesting with sublayouts
        topView_.draw();
        bottomView_.draw();
        tripleBoxView_.draw();
        bottomBoxView_.draw();
        evenSpacedWordsView_.draw();
        box1_.draw();
        box2_.draw();
        text1_.draw();
    }

protected:
    IconView iconView_;
    TextView topView_;
    TextView bottomView_;
    std::shared_ptr<TextView> nestedTextView_;
    HBox tripleBoxView_;
    HBox bottomBoxView_;
    HBox evenSpacedWordsView_;

    BoxView box1_;
    BoxView box2_;
    // BoxView box3_;
    TextView text1_;
};

// TODO: How to combine Screen and View? Is it:
// * Multiple inheritance?
// * Merge it all into View?
class LayoutTestScreen : public Screen {
public:
    LayoutTestScreen(DisplaySystem &display) : Screen(), view(display) {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

    ExampleView view;
};
