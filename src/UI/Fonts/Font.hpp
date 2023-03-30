#pragma once

#include <Adafruit_GFX.h>

enum FontType { GFX, IBMF };

class Font {
private:
    FontType type_;

public:
    inline auto isA(FontType fontType) const -> bool { return fontType == type_; }
    virtual auto yAdvance() const -> int = 0;

protected:
    Font(FontType fontType) : type_(fontType) {}
};

class GFont : public Font {
public:
    const GFXfont *font;

    GFont(const GFXfont &gfxFont) : Font(FontType::GFX), font(&gfxFont) {}

    inline auto get() const -> GFXfont const * { return font; }
    inline auto yAdvance() const -> int override { return font->yAdvance; }
};