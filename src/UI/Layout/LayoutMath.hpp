#pragma once

#include "Types.hpp"

auto PointToCenterX(const Size &size, const Size &inSize) -> Point;

auto PointToCenterY(const Size &size, const Size &inSize) -> Point;

auto AlignSizeInRect(const Size &size, const Rect &rect, int options) -> Rect;

class View;

auto SizeThatFitsForView(const View &view, Size size, int options) -> Size;

auto FrameForView(const View &view, Size size, Rect inFrame, int options) -> Rect;

auto OffsetFrame(const Rect &frame, const Point &offset) -> Rect;