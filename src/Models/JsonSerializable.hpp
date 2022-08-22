#pragma once

#include <ArduinoJson.h>

class JsonSerializable {
public:
    virtual auto fromJson(const JsonObject &j) -> bool = 0;
};
