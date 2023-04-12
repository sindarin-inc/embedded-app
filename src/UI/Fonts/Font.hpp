#pragma once

#include <Adafruit_GFX.h>

#include "sindarin-debug.h"

enum FontType { GFX, IBMF };

class Font {
private:
    FontType type_;

public:
    [[nodiscard]] auto isA(FontType fontType) const -> bool { return fontType == type_; }
    [[nodiscard]] virtual auto lineHeight() const -> int = 0;

protected:
    Font(FontType fontType) : type_(fontType) {}
};

class GFont : public Font {
public:
    const GFXfont *font;

    GFont(const GFXfont &gfxFont) : Font(FontType::GFX), font(&gfxFont) {}

    [[nodiscard]] inline auto get() const -> GFXfont const * { return font; }
    [[nodiscard]] auto lineHeight() const -> int override { return font->yAdvance; }
};