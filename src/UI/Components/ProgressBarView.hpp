#pragma once

#include "EinkView.hpp"

class ProgressBarView : public EinkView {
public:
    ProgressBarView(DisplaySystem &display, int cornerRadius = 0) : EinkView(display) {}
    ~ProgressBarView() override = default;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        return {size.width, 4};
    }

    void drawInRect(SolRect rect) override {
        display_.drawRoundRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height, 1,
                               display_.fg);

        if (progress_ > 0) {
            display_.fillRoundRect(rect.origin.x, rect.origin.y, rect.size.width * progress_ / 100,
                                   rect.size.height, 1, display_.fg);
        }
    }

    void setProgress(int progress) {
        if (progress == progress_) {
            return;
        }
        progress_ = std::clamp(progress, 0, 100);
    }

protected:
    // Progress from 0-100
    int progress_ = 0;
};
