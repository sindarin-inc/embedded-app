#pragma once

#include "Renderers/RendererStream.hpp"

auto IsPbm(const unsigned char *bitmap) -> bool;
auto ParsePbmHeader(RendererStream &stream, int &width, int &height) -> bool;