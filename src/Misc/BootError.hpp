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
        BLEWifi = 8,
        BLESindarin = 9,
        BLEManagerAdvertising = 10,
        WiFi = 11,
        Preferences = 12,
        AmbientLight = 13,
        Magnetometer = 14,
        LocalStore = 15,
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
        case BLEWifi:
            return "BLE/Wifi";
        case BLESindarin:
            return "BLE/Sindarin";
        case BLEManagerAdvertising:
            return "BLE/ManagerAdvertising";
        case WiFi:
            return "Wifi";
        case Preferences:
            return "Preferences";
        case AmbientLight:
            return "Ambient Light";
        case Magnetometer:
            return "Magnetometer";
        case LocalStore:
            return "Local Store";
        };
        return "Unhandled";
    }

    auto blinks() -> int { return type; }
};

void BlinkBootErrors(const std::vector<BootError> &bootErrors);
