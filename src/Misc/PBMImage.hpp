#pragma once

#include <Renderer/RendererStream.hpp>

auto ParsePbmHeader(RendererStream &stream, int &width, int &height) -> bool;