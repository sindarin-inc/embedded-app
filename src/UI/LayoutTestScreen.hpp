#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"

// A view that draws a line of text
class TextView : public View {
public:
    TextView(DisplaySystem &display, std::string text) : View(display), text_(std::move(text)) {}

    virtual auto layout(Size size) -> Size override {
        uint16_t w, h;
        display_.getTextBounds(text_.c_str(), &w, &h);
        return {w, h};
    }

    virtual void draw() override {
        layoutIfNeeded();
        display_.drawSingleLineOfText(text_, frame().origin.x, frame().origin.y);
    }

protected:
    std::string text_;
};

// A view that draws an icon
class IconView : public View {
public:
    IconView(DisplaySystem &display, unsigned char *iconBits, int width, int height)
        : View(display), iconBits_(iconBits), width_(width), height_(height) {}

    virtual auto layout(Size size) -> Size override { return {width_, height_}; }

    virtual void draw() override {
        display_.drawXBitmap(frame_.origin.x, frame_.origin.y, iconBits_, width_, height_,
                             display_.fg);
    }

protected:
    unsigned char *iconBits_;
    int width_;
    int height_;
};

// Example of a slightly more complex view that combines other views in a layout
class ExampleView : public View {
public:
    ExampleView(DisplaySystem &display)
        : View(display), iconView_(display, lightbulbBits, Lightbulb_width, Lightbulb_height),
          topView_(display, "First row of text"), bottomView_(display, "Second row") {}

    virtual auto layout(Size size) -> Size override {
        // inset all around by 2px
        int x = 2;
        int y = 2;
        size.width = size.width - 4;

        // Draw the icon vertically aligned on the left of the screen
        auto iconRect = layoutContext_.setViewFrame(
            iconView_, {{x, y}, size},
            LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignCenterVertical);

        // TODO: I think it might be possible to create virtual layout subgroupings inline here with
        // clever uses of lambdas. E.g. we could group the labels in a sub-layout, then align that
        // sub-layout as its own component, without having to create an separate view for the two
        // labels

        // Now on the same top pixel as the logo, draw the top text
        x = iconRect.origin.x + iconRect.size.width + 2;
        y = iconRect.origin.y;
        auto topTextRect = layoutContext_.setViewFrame(
            topView_, {{x, y}, size},
            LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignTop);

        // Draw the next line immediately below but with a 2px offset
        y += topTextRect.size.height + 2;
        auto bottomTextRect = layoutContext_.setViewFrame(
            bottomView_, {{x, y}, size},
            LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignTop);

        y += bottomTextRect.size.height;

        return {size.width, y};
    };

    virtual void draw() override {
        layoutIfNeeded();
        // TODO: We probably need a way to pass through the global coordinate system so that more
        // deeply nested views get drawn in the right place
        iconView_.draw();
        topView_.draw();
        bottomView_.draw();

        display_.drawBox(frame_.origin.x, frame_.origin.y, frame_.size.width, frame_.size.height);
    }

protected:
    IconView iconView_;
    TextView topView_;
    TextView bottomView_;
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
