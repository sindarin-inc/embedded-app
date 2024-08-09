
#include <Adafruit_GFX.h>
#include <array>
#include <cinttypes>

#include "FontEntry.hpp"
#include "config.h"

#if CONFIG_FONT_IBMF

// ----- IBMF Fonts -----

#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontData.hpp"
#include "IBMFFonts/SolSans_75.h"

// clang-format on

// Here is the list of all fonts that can be selected through a menu in the application.
//
// At boot time, the first entry in the list is loaded by default. If one of the other entries must
// be used to initialixe the font instance defined above, through a call to the font.load() method.
// For example:
//
//   result = font.load((fonts_defs::MemoryPtr)fontsList[0].content, fontsList[0].length,
//                fontsList[0].kerns, fontsList[0].ligatures);
//   if (result) {
//       ... was successfull
//   } else {
//       ... was not successfull
//   }
//
// The same approach can be used to change the current font through a selection menu

#define FONT_ENTRY(caption, name) {caption, name##_75_IBMF, name##_75_IBMF_LEN},

// clang-format off
std::array<FontEntry, 1> fontsList{{
    FONT_ENTRY("SolSans", SOLSANS)
}};

// The following defines the instance of a font that is containing all faces related to that font.
// The parameters are used to initialize the font at boot time to one of the fonts that is part of
// the list above. This is to insure that at any time, the font is loaded with one of the available
// entries. It can be changed using a call to font.load() method as shown in the comment of the
// fontsList array above.

FontData mainFont = FontData(fontsList[0].content, fontsList[0].length);

// The following definitions identifie three pre-defined empty font faces that are expected
// to be initialized through a call to font.load() method at application startup. It is expected
// that the selected font contains up to 3 face sizes. The numbers 0 to 2 are related to the face
// index inside the font. If an index doesn't exists, the driver will use the largest index
// available instead.
//
// PLEASE NOTE:
// If more faces are to be added, the MAX_FACE_COUNT definition in IBMFDefs.hpp must be adjusted 
// accordingly.

Font fontFace0 = Font(mainFont, 0);
Font fontFace1 = Font(mainFont, 1);
Font fontFace2 = Font(mainFont, 2);

#endif

#if CONFIG_FONT_TTF

// ----- TTF Fonts -----

#include "TTFDriver/TTFFont.hpp"
#include "TTFDriver/TTFNotoSansLight.hpp"

// clang-format on

// Here is the list of all fonts that can be selected through a menu in the application.
//
// At boot time, the first entry in the list is loaded by default. If one of the other entries must
// be used to initialixe the font instance defined above, through a call to the font.load() method.
// For example:
//
//   result = font.load((ibmf_defs::MemoryPtr)fontsList[0].content, fontsList[0].length,
//                fontsList[0].kerns, fontsList[0].ligatures); if (result) {
//       ... was successfull
//   } else {
//       ... was not successfull
//   }
//
// The same approach can be used to change the current font through a selection menu

// The following defines the instance of a font that is containing all faces related to that font.
// The parameters are used to initialize the font at boot time to one of the fonts that is part of
// the list above. This is to insure that at any time, the font is loaded with one of the available
// entries. It can be changed using a call to font.load() method as shown in the comment of the
// fontsList array above.

TTFNotoSansLight mainFont;

// The following definitions identifie three pre-defined empty font face sizes initialized at
// application startup. The second parameter is the face point size.

Font fontFace0 = Font(mainFont, 8);
Font fontFace1 = Font(mainFont, 10);
Font fontFace2 = Font(mainFont, 12);

#endif
