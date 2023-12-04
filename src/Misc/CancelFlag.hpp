#pragma once

#include <memory>

class CancelFlag {
public:
    CancelFlag() = default;
    CancelFlag(const CancelFlag &) = delete;
    CancelFlag &operator=(const CancelFlag &) = delete;

    static void cancel(const std::shared_ptr<CancelFlag> flag) {
        if (flag == nullptr) {
            return;
        }
        flag->cancelInternal();
    }

    static bool isCancelled(const std::shared_ptr<CancelFlag> flag) {
        if (flag == nullptr) {
            return false;
        }
        return flag->isCancelledInternal();
    }

private:
    bool cancelled_ = false;

    void cancelInternal() { cancelled_ = true; }
    bool isCancelledInternal() const { return cancelled_; }
};
