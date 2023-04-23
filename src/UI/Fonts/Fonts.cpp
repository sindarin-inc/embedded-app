
#include <Adafruit_GFX.h>
#include <array>
#include <cinttypes>

// ----- IBMF Fonts -----

#include "FontEntry.hpp"
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontLow.hpp"
#include "IBMFFonts/Arial_75.h"
#include "IBMFFonts/Baskerville_75.h"
#include "IBMFFonts/BeausiteClassic_75.h"
#include "IBMFFonts/ComicSans_75.h"
#include "IBMFFonts/DejaVuSansCondensed_75.h"
#include "IBMFFonts/DejaVuSans_75.h"
#include "IBMFFonts/FiraSans_75.h"
#include "IBMFFonts/FreeSans_75.h"
#include "IBMFFonts/GnuUnifont_75.h"
#include "IBMFFonts/Liberation_75.h"
#include "IBMFFonts/NotoSansLight_75.h"
#include "IBMFFonts/NotoSansMedium_75.h"
#include "IBMFFonts/NotoSans_75.h"
#include "IBMFFonts/OpenSansCondensed_75.h"
#include "IBMFFonts/OpenSans_75.h"
#include "IBMFFonts/RobotoCondensed_75.h"
#include "IBMFFonts/Roboto_75.h"
#include "IBMFFonts/SolSans_75.h"
#include "IBMFFonts/Tahoma_75.h"

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
std::array<FontEntry, 19> fontsList{{
    FONT_ENTRY("SolSans", SOL_SANS)
    FONT_ENTRY("GnuUnifont", GNUUNIFONT)
    FONT_ENTRY("Tahoma", TAHOMA)
    FONT_ENTRY("Arial", ARIAL)
    FONT_ENTRY("Baskerville", BASKERVILLE)
    FONT_ENTRY("Beausite", BEAUSITECLASSIC)
    FONT_ENTRY("Comic", COMICSANS)
    FONT_ENTRY("DejaVu", DEJAVUSANS)
    FONT_ENTRY("DejaVuCondensed", DEJAVUSANSCONDENSED)
    FONT_ENTRY("Fira", FIRASANS)
    FONT_ENTRY("Free", FREESANS)
    FONT_ENTRY("Liberation", LIBERATION)
    FONT_ENTRY("Noto", NOTOSANS)
    FONT_ENTRY("NotoLight", NOTOSANSLIGHT)
    FONT_ENTRY("NotoMedium", NOTOSANSMEDIUM)
    FONT_ENTRY("Open", OPENSANS)
    FONT_ENTRY("OpenCondensed", OPENSANSCONDENSED)
    FONT_ENTRY("Roboto", ROBOTO)
    FONT_ENTRY("RobotoCondensed", ROBOTOCONDENSED)
}};
// clang-format on

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

// ----- GFX Fonts -----

#include "GFXFonts/LucidaGrande10pt7b.h"
#include "GFXFonts/LucidaGrande14pt7b.h"
#include "GFXFonts/LucidaGrande5pt7b.h"
#include "GFXFonts/LucidaGrande6pt7b.h"
#include "GFXFonts/Tahoma10pt7b.h"
#include "GFXFonts/Tahoma20pt7b.h"
#include "GFXFonts/Tahoma5pt7b.h"
#include "GFXFonts/Tahoma6pt7b.h"
#include "GFXFonts/Tahoma7pt7b.h"
#include "GFXFonts/Tahoma8pt7b.h"
#include "GFXFonts/Tahoma9pt7b.h"

GFont fontLucidaGrandE5PT7B = GFont(LUCIDA_GRANDE5PT7B);
GFont fontLucidaGrandE5PT7BSkinny = GFont(LUCIDA_GRANDE5PT7B_SKINNY);
GFont fontLucidaGrandE5PT7BFat = GFont(LUCIDA_GRANDE5PT7B_FAT);
GFont fontLucidaGrandE6PT7B = GFont(LUCIDA_GRANDE6PT7B);
GFont fontLucidaGrandE10PT7B = GFont(LUCIDA_GRANDE10PT7B);
GFont fontLucidaGrandE14PT7B = GFont(LUCIDA_GRANDE14PT7B);
GFont fontTahomaRegularFonT5PT7B = GFont(TAHOMA_REGULAR_FONT5PT7B);
GFont fontTahomaRegularFonT6PT7B = GFont(TAHOMA_REGULAR_FONT6PT7B);
GFont fontTahomaRegularFonT7PT7B = GFont(TAHOMA_REGULAR_FONT7PT7B);
GFont fontTahomaRegularFonT8PT7B = GFont(TAHOMA_REGULAR_FONT8PT7B);
GFont fontTahomaRegularFonT9PT7B = GFont(TAHOMA_REGULAR_FONT9PT7B);
GFont fontTahomaRegularFonT10PT7B = GFont(TAHOMA_REGULAR_FONT10PT7B);
GFont fontTahomaRegularFonT20PT7B = GFont(TAHOMA_REGULAR_FONT10PT7B);
