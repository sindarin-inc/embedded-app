#pragma once

#include <ArduinoJson.h>
#include <Spiram/String.hpp>

namespace ARDUINOJSON_NAMESPACE {
template <>
struct IsString<SpiramString> : true_type {};
} // namespace ARDUINOJSON_NAMESPACE
