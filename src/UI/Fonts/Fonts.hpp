#pragma once

#include "Font.hpp"
#include "FontEntry.hpp"

extern std::array<FontEntry, 1> fontsList;

#if IBMF_SUPPORT
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontData.hpp"

// For the IBMF Fonts, a single file contains multiple "face" sizes. A single
// object is created using the IBMFFontData class that manage the whole .h file
// and "faces" are hooked through the IBMFFont class.

// All the fonts are instanciated in Fonts.cpp

extern FontData ibmf75;

extern Font fontFace0;
extern Font fontFace1;
extern Font fontFace2;

#endif

#if TTF_SUPPORT
#include "TTFDriver/TTFFont.hpp"
#include "TTFDriver/TTFFontData.hpp"

// For the IBMF Fonts, a single file contains multiple "face" sizes. A single
// object is created using the IBMFFontData class that manage the whole .h file
// and "faces" are hooked through the IBMFFont class.

// All the fonts are instanciated in Fonts.cpp

extern FontData notoSans;

extern Font fontFace0;
extern Font fontFace1;
extern Font fontFace2;

#endif