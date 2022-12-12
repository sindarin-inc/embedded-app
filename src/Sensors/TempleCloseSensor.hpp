#include <functional>
#include <utility>

#include "Mmc5603.hpp"

class TempleCloseSensor {
public:
    TempleCloseSensor() : magSensor_(0) {}

    void checkTemple();

    // Simple diagnostic test to check if the magnetometer is reporting the correct product ID
    auto checkProductId() -> bool;

    void setCallback(std::function<void(bool opened)> callback) { callback_ = std::move(callback); }

    auto getOpened() -> bool { return opened_; }

protected:
    Mmc5603 magSensor_;
    bool opened_ = false;
    std::function<void(bool opened)> callback_;
};
