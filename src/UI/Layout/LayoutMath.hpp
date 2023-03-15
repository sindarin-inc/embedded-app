#pragma once

#include "Types.hpp"

class LayoutContext;

auto AlignSizeInRect(const Size &size, const Rect &rect, int options) -> Rect;

auto SizeThatFitsForLayout(const LayoutContext &layoutContext, Size size, int options) -> Size;

auto FrameForLayout(const LayoutContext &layoutContext, Size size, Rect inFrame, int options)
    -> Rect;
