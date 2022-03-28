#include <string>
#include <vector>

struct BootError {
    enum Type {
        // Skip 1 since 1 blink might be hard to see
        PowerManagement = 2,
        FileSystem = 3,
        LowPowerOscillator = 4,
        ExternalRam = 5,
        Display = 6,
        BLE = 7,
    };
    Type type;

    auto string() -> std::string {
        switch (type) {
        case PowerManagement:
            return "Power Management";
        case FileSystem:
            return "File System";
        case LowPowerOscillator:
            return "Low-power Oscillator (32.768kHz XTAL)";
        case ExternalRam:
            return "External PSRAM";
        case Display:
            return "Display";
        case BLE:
            return "BLE";
        };
        return "Unhandled";
    }

    auto blinks() -> int { return type; }
};

void BlinkBootErrors(const std::vector<BootError> &bootErrors);
