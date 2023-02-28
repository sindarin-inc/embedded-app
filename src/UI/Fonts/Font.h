#pragma once

#include <Adafruit_GFX.h>

enum FontType { GFX, IBMF };

class Font {
private:
    FontType type;

public:
    inline bool is_a(FontType font_type) const { return font_type == type; };
    virtual int yAdvance() const = 0;

protected:
    Font(FontType font_type) : type(font_type) {}
};

class GFont : public Font {
public:
    const GFXfont *font;

    GFont(const GFXfont &gfx_font) : Font(FontType::GFX), font(&gfx_font) {}

    inline GFXfont const *get() const { return font; }
    inline int yAdvance() const { return font->yAdvance; }
};

class IBMFFont;

class IFont : Font {
public:
    IBMFFont *font;

    IFont(IBMFFont &ibmf_font) : Font(FontType::IBMF), font(&ibmf_font) {}

    inline IBMFFont *get() const { return font; }
    inline int yAdvance() const { return 10; } // TODO: Adjust with the IBMFFont...
};
