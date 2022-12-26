#pragma once

#include "Types.hpp"

auto PointToCenterX(const Size &size, const Size &inSize) -> Point;

auto PointToCenterY(const Size &size, const Size &inSize) -> Point;

auto AlignSizeInRect(const Size &size, const Rect &rect, int options) -> Rect;

class LayoutContext;

auto SizeThatFitsForLayout(const LayoutContext &layoutContext, Size size, int options) -> Size;

auto FrameForLayout(const LayoutContext &layoutContext, Size size, Rect inFrame, int options)
    -> Rect;

auto OffsetFrame(const Rect &frame, const Point &offset) -> Rect;
