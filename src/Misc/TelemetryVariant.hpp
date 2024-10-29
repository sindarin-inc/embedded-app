#include <string>
#include <variant>

#include "Misc/SpiramAllocator.hpp"

typedef std::variant<int, uint16_t, uint32_t, float, std::string, SpiramString> TelemetryVariant;
