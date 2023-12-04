#pragma once

#include <memory>

class CancelFlag {
public:
    CancelFlag() = default;
    CancelFlag(const CancelFlag &) = delete;
    auto operator=(const CancelFlag &) -> CancelFlag & = delete;

    static void cancel(const std::shared_ptr<CancelFlag> &flag) {
        if (flag == nullptr) {
            return;
        }
        flag->cancelInternal();
    }

    static auto isCancelled(const std::shared_ptr<CancelFlag> &flag) -> bool {
        if (flag == nullptr) {
            return false;
        }
        return flag->isCancelledInternal();
    }

private:
    bool cancelled_ = false;

    void cancelInternal() { cancelled_ = true; }
    [[nodiscard]] auto isCancelledInternal() const -> bool { return cancelled_; }
};
