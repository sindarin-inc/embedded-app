#include <string>
#include <variant>

#include "Misc/SpiramAllocator.hpp"

typedef std::variant<int, uint32_t, float, const char *, std::string, SpiramString>
    TelemetryVariant;
