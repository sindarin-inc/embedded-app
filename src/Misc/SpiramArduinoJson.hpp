#pragma once

#include <ArduinoJson.h>

#include "SpiramAllocator.hpp"

namespace ARDUINOJSON_NAMESPACE {
template <>
struct IsString<SpiramString> : true_type {};
} // namespace ARDUINOJSON_NAMESPACE
