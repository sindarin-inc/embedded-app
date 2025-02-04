#pragma once

#include <memory>
#include <stack>
#include <stdexcept>

#include "UI/Screen.hpp"

class ScreenStack : public std::stack<std::shared_ptr<Screen>> {
public:
    void pop() {
        if (this->empty()) {
            log_e("ScreenStack is empty");
            return;
        }
        std::stack<std::shared_ptr<Screen>>::pop();
        if (!this->empty()) {
            this->top()->setFirstLoad();
            this->top()->setNeedsRerender();
        }
    }
};
