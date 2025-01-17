#include "LayoutMath.hpp"

#include "LayoutContext.hpp"

auto AlignSizeInRect(const SolSize &size, const SolRect &rect, int options) -> SolRect {
    SolPoint p = rect.origin;

    if ((options & LayoutOptionsAlignCenterHorizontal) != 0) {
        p.x += (rect.size.width - size.width) / 2;
    }

    if ((options & LayoutOptionsAlignCenterVertical) != 0) {
        p.y += (rect.size.height - size.height) / 2;
    }

    if ((options & LayoutOptionsAlignRight) != 0) {
        p.x = rect.origin.x + rect.size.width - size.width;
    }

    if ((options & LayoutOptionsAlignBottom) != 0) {
        p.y = rect.origin.y + rect.size.height - size.height;
    }

    return {{p.x, p.y}, {size.width, size.height}};
}

auto SizeThatFitsForLayout(const LayoutContext &layoutContext, SolSize size, int options)
    -> SolSize {
    SolSize originalSize = size;
    bool sizeToFit = ((options & LayoutOptionsSizeToFitHorizontal) != 0) ||
                     ((options & LayoutOptionsSizeToFitVertical) != 0);
    if (sizeToFit) {
        // Dangerous cast to non-const LayoutContext because we are guaranteeing that when
        // sizing==true we are not going to modify anything
        SolSize sizeThatFits =
            const_cast<LayoutContext &>(layoutContext).layoutIfNeeded(size, true);

        // If size that fits returns a larger width, then we'll need to constrain it.
        if (((options & LayoutOptionsConstrainWidth) != 0) && sizeThatFits.width > size.width) {
            sizeThatFits.width = size.width;
        }

        // If size that fits returns a larger height, then we'll need to constrain it.
        if (((options & LayoutOptionsConstrainHeight) != 0) && sizeThatFits.height > size.height) {
            sizeThatFits.height = size.height;
        }

        // If size that fits returns a larger width or height, constrain it, but also
        // maintain the aspect ratio from sizeThatFits
        if (((options & LayoutOptionsConstrainSizeMaintainAspectRatio) != 0) &&
            (sizeThatFits.height > size.height || sizeThatFits.width > size.width)) {
            float aspectRatio =
                static_cast<float>(sizeThatFits.width) / static_cast<float>(sizeThatFits.height);
            // If we're going to constrain by width
            if (sizeThatFits.width / size.width > sizeThatFits.height / size.height) {
                sizeThatFits.width = size.width;
                sizeThatFits.height = size.width / aspectRatio;
            } else {
                sizeThatFits.height = size.height;
                sizeThatFits.width = size.height * aspectRatio;
            }
        }

        // If size that fits returns a smaller width, then we'll need to use the default
        // width.
        if (((options & LayoutOptionsDefaultWidth) != 0) && sizeThatFits.width == 0) {
            sizeThatFits.width = size.width;
        }

        // If size that fits returns a smaller height, then we'll need to use the default
        // height.
        if (((options & LayoutOptionsDefaultHeight) != 0) && sizeThatFits.height == 0) {
            sizeThatFits.height = size.height;
        }

        size = sizeThatFits;
    }

    if ((options & LayoutOptionsSizeToFitHorizontal) == 0) {
        size.width = originalSize.width;
    }

    if ((options & LayoutOptionsSizeToFitVertical) == 0) {
        size.height = originalSize.height;
    }

    return size;
}

auto FrameForLayout(const LayoutContext &layoutContext, SolSize size, SolRect inRect, int options)
    -> SolRect {
    size = SizeThatFitsForLayout(layoutContext, size, options);
    if (inRect.size.width == 0) {
        inRect.size.width = size.width;
    }
    if (inRect.size.height == 0) {
        inRect.size.height = size.height;
    }
    return AlignSizeInRect(size, inRect, options);
}
