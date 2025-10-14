#include <Spiram/String.hpp>
#include <string>
#include <variant>

typedef std::variant<int, uint16_t, uint32_t, float, std::string, SpiramString> TelemetryVariant;
