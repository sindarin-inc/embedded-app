#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/Firmware.hpp"

class FirmwareResponse : JsonSerializable {
public:
    FirmwareResponse() = default;
    ~FirmwareResponse() override = default;

    Firmware latestGlassesFirmware;
    // Unused for now. Skip parsing it.
    // Firmware latestPtFirmware;

    auto fromJson(const JsonObject &j) -> bool override {
        return latestGlassesFirmware.fromJson(j["latest_glasses_firmware"]);
    }
};
