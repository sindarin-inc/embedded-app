#pragma once

struct Point {
    int x;
    int y;
    static auto zero() -> Point { return {0, 0}; }

    auto operator==(const Point &other) const -> bool { return x == other.x && y == other.y; }
    auto operator!=(const Point &other) const -> bool { return x != other.x || y != other.y; }
};

struct Size {
    int width;
    int height;
    static auto zero() -> Size { return {0, 0}; }

    auto operator==(const Size &other) const -> bool {
        return width == other.width && height == other.height;
    }
    auto operator!=(const Size &other) const -> bool {
        return width != other.width || height != other.height;
    }
};

struct Rect {
    Point origin;
    Size size;
    static auto zero() -> Rect { return {{0, 0}, {0, 0}}; }
    static auto fromSize(Size size) -> Rect { return {{0, 0}, size}; }

    [[nodiscard]] auto inset(int left, int right, int top, int bottom) const -> Rect {
        return {{origin.x + left, origin.y + top},
                {size.width - left - right, size.height - top - bottom}};
    }
    [[nodiscard]] auto inset(int xAmount, int yAmount) const -> Rect {
        return inset(xAmount, xAmount, yAmount, yAmount);
    }
    [[nodiscard]] auto inset(int amount) const -> Rect { return inset(amount, amount); }
    [[nodiscard]] auto offset(const Point &offset) const -> Rect {
        return {{origin.x + offset.x, origin.y + offset.y}, size};
    }
    [[nodiscard]] auto left() const -> int { return origin.x; }
    [[nodiscard]] auto right() const -> int { return origin.x + size.width; }
    [[nodiscard]] auto top() const -> int { return origin.y; }
    [[nodiscard]] auto bottom() const -> int { return origin.y + size.height; }

    auto operator==(const Rect &other) const -> bool {
        return origin == other.origin && size == other.size;
    }
    auto operator!=(const Rect &other) const -> bool {
        return origin != other.origin || size != other.size;
    }
};

enum LayoutOptions {
    LayoutOptionsNone = 0,

    // `SizeToFit` options: Allow view to size to its desired size based on its content

    //! Size the view to fit vertically
    LayoutOptionsSizeToFitVertical = 1 << 0,
    //! Size the view to fit horizontally
    LayoutOptionsSizeToFitHorizontal = 1 << 1,
    //! Size the view to fit both horizontally and vertically
    LayoutOptionsSizeToFit = LayoutOptionsSizeToFitVertical | LayoutOptionsSizeToFitHorizontal,

    // `Constrain` options: Constrain view's desired size to the passed-in size

    // Constrain sizeThatFit's width to the passed in frame
    LayoutOptionsConstrainWidth = 1 << 2,
    // Constrain sizeThatFit's height to the passed in frame
    LayoutOptionsConstrainHeight = 1 << 3,
    // Constrain sizeThatFit's height and width
    LayoutOptionsConstrainSize = LayoutOptionsConstrainWidth | LayoutOptionsConstrainHeight,
    // Constrain sizeThatFits to the size specified but maintain the original aspect ratio.
    LayoutOptionsConstrainSizeMaintainAspectRatio = 1 << 4,

    // `Default` options: provide default values for the size if the view returns a 0 size

    // Whether the height specified is the default. Using this option will use the specified
    // height when the layout method returns a 0 height.
    LayoutOptionsDefaultHeight = 1 << 5,
    // Whether the width specified is the default. Using this option will use the specified
    // width when the layout method returns a 0 width.
    LayoutOptionsDefaultWidth = 1 << 6,
    // Whether the size specified is the default. Using this option will use the specified width
    // and/or height when sizeThatFits: returns a 0 width and/or height, respectively.
    // This is useful for views that load their content asynchronously and may initially not know
    // their size.
    LayoutOptionsDefaultSize = LayoutOptionsDefaultHeight | LayoutOptionsDefaultWidth,

    // `Align` options: Align the view within the passed-in rect

    // Center size vertically in the passed in rect
    LayoutOptionsAlignCenterVertical = 1 << 10,
    // Center size horizontally in the passed in rect
    LayoutOptionsAlignCenterHorizontal = 1 << 11,
    // Center size vertically and horizontally in the passed in rect
    LayoutOptionsAlignCenter =
        LayoutOptionsAlignCenterVertical | LayoutOptionsAlignCenterHorizontal,
    // Align size with the right of the passed in rect
    LayoutOptionsAlignRight = 1 << 12,
    // Align size with the bottom of the passed in rect
    LayoutOptionsAlignBottom = 1 << 13,
    // Align size with the left of the passed in rect
    // NOTE: This option is the default behavior of the coordinate system so don't need to be set,
    // but is included for completeness
    LayoutOptionsAlignLeft = 0,
    // Align size with the top of the passed in rect
    // NOTE: This option is the default behavior of the coordinate system so don't need to be set,
    // but is included for completeness
    LayoutOptionsAlignTop = 0,
};
