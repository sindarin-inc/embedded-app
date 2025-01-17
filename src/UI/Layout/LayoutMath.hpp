#pragma once

#include "Types.hpp"

class LayoutContext;

auto AlignSizeInRect(const SolSize &size, const SolRect &rect, int options) -> SolRect;

auto SizeThatFitsForLayout(const LayoutContext &layoutContext, SolSize size, int options)
    -> SolSize;

auto FrameForLayout(const LayoutContext &layoutContext, SolSize size, SolRect inRect, int options)
    -> SolRect;
