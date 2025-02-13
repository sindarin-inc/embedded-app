#pragma once

#include <ArduinoJson.h>

#include "Misc/SpiramArduinoJson.hpp"

class JsonSerializable {
public:
    virtual ~JsonSerializable() = default;
    virtual auto fromJson(const JsonObject &j) -> bool = 0;
};
