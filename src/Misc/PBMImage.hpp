#pragma once

#include "Renderers/RendererStream.hpp"

auto isPBM(const unsigned char *bitmap) -> bool;
auto ParsePbmHeader(RendererStream &stream, int &width, int &height) -> bool;