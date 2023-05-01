#include <functional>
#include <optional>
#include <utility>

#include "Mmc5603.hpp"

// TODO: Consider collapsing this into PowerManager
class TempleCloseSensor {
public:
    TempleCloseSensor() : magSensor_(0) {}
    TempleCloseSensor(TempleCloseSensor const &) = delete;

    // Simple diagnostic test to check if the magnetometer is reporting the correct product ID
    auto checkProductId() -> bool;

    // check the magnetometer to determine if the temple is open.
    auto isTempleOpen() -> bool;

    // Should be called periodically to fire the callback when the temple is opened or closed (only
    // fires the callback on state change, not on the initial reading)
    void checkTemple();

    void setCallback(std::function<void(bool opened)> callback) { callback_ = std::move(callback); }

protected:
    Mmc5603 magSensor_;
    // std::optional so we don't trigger the callback on the very first state, only on state changes
    std::optional<bool> opened_;
    std::function<void(bool opened)> callback_;
};
