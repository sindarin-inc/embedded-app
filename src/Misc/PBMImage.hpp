#pragma once

#include "Renderers/RendererStream.hpp"

auto ParsePbmHeader(RendererStream &stream, int &width, int &height) -> bool;