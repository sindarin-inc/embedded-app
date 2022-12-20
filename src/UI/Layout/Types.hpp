#pragma once

struct Point {
    int x;
    int y;
    static Point zero() { return {0, 0}; }
};

struct Size {
    int width;
    int height;
    static Size zero() { return {0, 0}; }
    // Overload == operator
    bool operator==(const Size &other) const {
        return width == other.width && height == other.height;
    }
};

struct Rect {
    Point origin;
    Size size;
    static Rect zero() { return {{0, 0}, {0, 0}}; }
};

enum LayoutOptions {
    LayoutOptionsNone = 0,

    // SIZING
    //! Size the view to fit vertically
    LayoutOptionsSizeToFitVertical = 1 << 0,
    //! Size the view to fit horizontally
    LayoutOptionsSizeToFitHorizontal = 1 << 1,
    //! Size the view to fit both horizontally and vertically
    LayoutOptionsSizeToFit = LayoutOptionsSizeToFitVertical | LayoutOptionsSizeToFitHorizontal,

    /*!
     Constrain width to the passed in frame.
     */
    LayoutOptionsConstrainWidth = 1 << 2,
    //! Constrain sizeThatFit's height to the passed in frame
    LayoutOptionsConstrainHeight = 1 << 3,
    //! Constrain sizeThatFit's height and width
    LayoutOptionsConstrainSize = LayoutOptionsConstrainWidth | LayoutOptionsConstrainHeight,

    /*!
     Constrain sizeThatFits to the size specified but maintain the original aspect ratio.

     This is useful for an image view that may need to be constrained to a max size, but still
     maintain its aspect ratio.
     */
    LayoutOptionsConstrainSizeMaintainAspectRatio = 1 << 4,

    //! Whether the height specified is the default. Using this option will use the specified
    //! height when sizeThatFits: returns a 0 height.
    LayoutOptionsDefaultHeight = 1 << 5,
    //! Whether the width specified is the default. Using this option will use the specified
    //! width when sizeThatFits: returns a 0 width.
    LayoutOptionsDefaultWidth = 1 << 6,
    /*!
     Whether the size specified is the default. Using this option will use the specified width
     and/or height when sizeThatFits: returns a 0 width and/or height, respectively.

     This is useful for an image view that has to load its data and may initially return 0 for
     sizeThatFits.
     */
    LayoutOptionsDefaultSize = LayoutOptionsDefaultHeight | LayoutOptionsDefaultWidth,

    // ALIGNMENT
    //! After sizing, center vertically in the passed in rect
    LayoutOptionsAlignCenterVertical = 1 << 10,
    //! After sizing, center horizontally in the passed in rect
    LayoutOptionsAlignCenterHorizontal = 1 << 11,
    //! After sizing, center vertically and horizontally in the passed in rect
    LayoutOptionsAlignCenter =
        LayoutOptionsAlignCenterVertical | LayoutOptionsAlignCenterHorizontal,
    //! After sizing, aligns the view with the right of the passed in rect
    LayoutOptionsAlignRight = 1 << 12,
    //! After sizing, aligns the view with the bottom of the passed in rect
    LayoutOptionsAlignBottom = 1 << 13,
    // These are the default behavior of the coordinate system so don't need to be flags, but
    // putting them here for completeness
    LayoutOptionsAlignLeft = 0,
    LayoutOptionsAlignTop = 0,
};
