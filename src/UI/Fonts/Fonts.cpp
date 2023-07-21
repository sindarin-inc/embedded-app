
#include <Adafruit_GFX.h>
#include <array>
#include <cinttypes>

// ----- IBMF Fonts -----

#include "FontEntry.hpp"
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontLow.hpp"
#include "IBMFFonts/SolSans_75.h"

// clang-format on

// Here is the list of all fonts that can be selected through a menu in the application.
//
// At boot time, one of these entries must be used to initialixe the ibmf75 font instance
// defined above, through a call to the ibmf75.load() method. For example:
//
//   result = ibmf75.load((ibmf_defs::MemoryPtr)fontsList[0].content, fontsList[0].length);
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
// entries. It can be changed using a call to ibmf75.load() method as shown in the comment of the
// fontsList array above.

IBMFFontLow ibmf75 = IBMFFontLow(fontsList[0].content, fontsList[0].length);

// The following definitions identifie four pre-defined empty font faces that are expected
// to be initialized through a call to ibmf75.load() method at application startup. It is expected
// that the selected font contains up to 4 face sizes. The numbers 0 to 3 are related to the face
// index inside the font. If an index doesn't exists, the driver will use the largest index
// available instead.

IBMFFont ibmf75Face0 = IBMFFont(ibmf75, 0);
IBMFFont ibmf75Face1 = IBMFFont(ibmf75, 1);
IBMFFont ibmf75Face2 = IBMFFont(ibmf75, 2);
IBMFFont ibmf75Face3 = IBMFFont(ibmf75, 3);
