#pragma once

#include "EinkView.hpp"

// Evenly spaced set of views in one dimension or the other
class StackView : public EinkView {
public:
    StackView(DisplaySystem &display, std::vector<std::shared_ptr<EinkView>> views, int spacing = 2,
              bool vertical = false)
        : EinkView(display), views_(std::move(views)), spacing_(spacing), vertical_(vertical) {}
    ~StackView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        // Assume equal sizes for now
        int viewCount = views_.size();
        int dimension = vertical_ ? size.height : size.width;

        Size boxSize{};
        if (vertical_) {
            int boxHeight = (dimension - (viewCount - 1) * spacing_) / viewCount;
            boxSize = {size.width, boxHeight};
        } else {
            int boxWidth = (dimension - (viewCount - 1) * spacing_) / viewCount;
            boxSize = {boxWidth, size.height};
        }
        int x = 0;
        int y = 0;

        for (auto &view : views_) {
            context.setViewFrame(*view, {{x, y}, boxSize},
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                     LayoutOptionsDefaultSize);
            if (vertical_) {
                y += boxSize.height + spacing_;
            } else {
                x += boxSize.width + spacing_;
            }
        }

        return size;
    }

    void drawInRect(Rect rect) override {
        for (auto &view : views_) {
            (*view).draw();
        }
    }

protected:
    std::vector<std::shared_ptr<EinkView>> views_;
    int spacing_;
    bool vertical_;
};
