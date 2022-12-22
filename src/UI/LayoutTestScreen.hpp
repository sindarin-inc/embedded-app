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

    auto layout(Size size) -> Size override {
        uint16_t w, h;
        display_.getTextBounds(text_.c_str(), &w, &h);
        return {w, display_.getFont()->yAdvance};
    }

    void drawInRect(Rect rect) override {
        log_d("Draw TextView in rect %d %d %d %d", rect.origin.x, rect.origin.y, rect.size.width,
              rect.size.height);
        View::drawInRect(rect);
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

    auto layout(Size size) -> Size override { return {width_, height_}; }

    void drawInRect(Rect rect) override {
        View::drawInRect(rect);
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

    auto layout(Size size) -> Size override { return size; }

    void drawInRect(Rect rect) override {
        log_d("Draw BoxView in rect %d %d %d %d", rect.origin.x, rect.origin.y, rect.size.width,
              rect.size.height);
        View::drawInRect(rect);
        display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                          display_.fg);
    }
};

// Just draws three boxes horizontally size by side
class HBox : public EinkView {
public:
    HBox(DisplaySystem &display, std::vector<std::shared_ptr<EinkView>> views, int spacing = 2)
        : EinkView(display), views_(views), spacing_(spacing) {}

    auto layout(Size size) -> Size override {
        log_d("HBox layout size %d %d", size.width, size.height);

        // Assume equal sizes for now
        int viewCount = views_.size();
        int boxWidth = (size.width - (viewCount - 1) * spacing_) / viewCount;
        int x = 0;
        int y = 0;

        for (auto &view : views_) {
            layoutContext_.setViewFrame(*view, {{x, y}, {boxWidth, size.height}},
                                        LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
            x += boxWidth + spacing_;
        }

        return size;
    }

    void drawInRect(Rect rect) override {
        log_d("Draw HBox in rect %d %d %d %d", rect.origin.x, rect.origin.y, rect.size.width,
              rect.size.height);
        View::drawInRect(rect);
        for (auto &view : views_) {
            (*view).drawInParent(*this);
        }
    }

protected:
    std::vector<std::shared_ptr<EinkView>> views_;
    int spacing_;
};

// Example of a slightly more complex view that combines other views in a layout
class ExampleView : public EinkView {
public:
    ExampleView(DisplaySystem &display)
        : EinkView(display), iconView_(display, lightbulbBits, Lightbulb_width, Lightbulb_height),
          topView_(display, "First row of text"), bottomView_(display, "Second row"),
          tripleBoxView_(display,
                         {std::make_shared<BoxView>(display),
                          std::make_shared<HBox>(display,
                                                 (std::vector<std::shared_ptr<EinkView>>){
                                                     std::make_shared<BoxView>(display),
                                                     std::make_shared<BoxView>(display)})}),
          bottomBoxView_(display,
                         {std::make_shared<BoxView>(display), std::make_shared<BoxView>(display),
                          std::make_shared<TextView>(display, "HIII")}),
          evenSpacedWordsView_(display, {std::make_shared<TextView>(display, "its"),
                                         std::make_shared<TextView>(display, "me,"),
                                         std::make_shared<TextView>(display, "hi"),
                                         std::make_shared<TextView>(display, "i'm"),
                                         std::make_shared<TextView>(display, "the")}) {}

    auto layout(Size size) -> Size override {
        log_d("ExampleView layout size %d %d", size.width, size.height);
        // inset all around by 2px
        int x = 2;
        int y = 2;
        size.width = size.width - 4;

        // Draw the icon vertically aligned on the left of the screen
        auto iconRect = layoutContext_.setViewFrame(
            iconView_, {{x, y}, size},
            LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignCenterVertical);

        // TODO: I think it might be possible to create virtual layout subgroupings
        // inline here with clever uses of lambdas. E.g. we could group the labels
        // in a sub-layout, then align that sub-layout as its own component, without
        // having to create an separate view for the two labels

        // Now on the same top pixel as the logo, draw the top text
        x = iconRect.origin.x + iconRect.size.width + 2;
        y = iconRect.origin.y;
        int textWidth = size.width - x;
        auto topTextRect = layoutContext_.setViewFrame(
            topView_, {{x, y}, {textWidth, size.height}},
            LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignTop);

        // Draw the next line immediately below but with a 2px offset
        y += topTextRect.size.height;
        auto bottomTextRect = layoutContext_.setViewFrame(
            bottomView_, {{x, y}, {textWidth, size.height}},
            LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignTop);

        y += bottomTextRect.size.height;
        auto tripleBoxRect = layoutContext_.setViewFrame(tripleBoxView_, {{x, y}, {textWidth, 10}});

        y += tripleBoxRect.size.height + 2;
        auto bottomBoxRect = layoutContext_.setViewFrame(bottomBoxView_, {{x, y}, {textWidth, 15}});

        y += bottomBoxRect.size.height + 2;
        auto evenSpacedWordsRect =
            layoutContext_.setViewFrame(evenSpacedWordsView_, {{x, y}, {textWidth, 15}});

        return {size.width, y};
    };

    void drawInRect(Rect rect) override {
        log_d("Draw ExampleView in rect %d %d %d %d", rect.origin.x, rect.origin.y, rect.size.width,
              rect.size.height);
        View::drawInRect(rect);
        // TODO: We probably need a way to pass through the global coordinate system
        // so that more deeply nested views get drawn in the right place
        iconView_.drawInParent(*this);
        topView_.drawInParent(*this);
        bottomView_.drawInParent(*this);
        tripleBoxView_.drawInParent(*this);
        bottomBoxView_.drawInParent(*this);
        evenSpacedWordsView_.drawInParent(*this);
    }

protected:
    IconView iconView_;
    TextView topView_;
    TextView bottomView_;
    HBox tripleBoxView_;
    HBox bottomBoxView_;
    HBox evenSpacedWordsView_;
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
