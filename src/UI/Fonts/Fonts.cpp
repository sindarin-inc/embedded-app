
#include <Adafruit_GFX.h>
#include <cinttypes>

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
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontLow.hpp"
//#include "IBMFFonts/Tahoma_75.h"

IBMFFontLow test75 = IBMFFontLow();

#define FACES(name)                                                                                \
    IBMFFont fontTest75BPI9PT = IBMFFont(test75, name##_75_IBMF, name##_75_IBMF_LEN, 0);           \
    IBMFFont fontTest75BPI10PT = IBMFFont(test75, name##_75_IBMF, name##_75_IBMF_LEN, 1);          \
    IBMFFont fontTest75BPI12PT = IBMFFont(test75, name##_75_IBMF, name##_75_IBMF_LEN, 2);          \
    IBMFFont fontTest75BPI14PT = IBMFFont(test75, name##_75_IBMF, name##_75_IBMF_LEN, 3);

// As the GNU Font possesses only one face size, the four default faces are pointing to the same
// one.
#define ONE_FACE(name)                                                                             \
    IBMFFont fontTest75BPI9PT = IBMFFont(test75, name##_75_IBMF, name##_75_IBMF_LEN, 0);           \
    IBMFFont fontTest75BPI10PT = IBMFFont(test75, name##_75_IBMF, name##_75_IBMF_LEN, 0);          \
    IBMFFont fontTest75BPI12PT = IBMFFont(test75, name##_75_IBMF, name##_75_IBMF_LEN, 0);          \
    IBMFFont fontTest75BPI14PT = IBMFFont(test75, name##_75_IBMF, name##_75_IBMF_LEN, 0);

#include "IBMFFonts/GnuUnifont_75.h"
ONE_FACE(GNUUNIFONT)

GFont FONT_LUCIDA_GRANDE5PT7B = GFont(LUCIDA_GRANDE5PT7B);
GFont FONT_LUCIDA_GRANDE5PT7B_SKINNY = GFont(LUCIDA_GRANDE5PT7B_SKINNY);
GFont FONT_LUCIDA_GRANDE5PT7B_FAT = GFont(LUCIDA_GRANDE5PT7B_FAT);
GFont FONT_LUCIDA_GRANDE6PT7B = GFont(LUCIDA_GRANDE6PT7B);
GFont FONT_LUCIDA_GRANDE10PT7B = GFont(LUCIDA_GRANDE10PT7B);
GFont FONT_LUCIDA_GRANDE14PT7B = GFont(LUCIDA_GRANDE14PT7B);
GFont FONT_TAHOMA_REGULAR_FONT5PT7B = GFont(TAHOMA_REGULAR_FONT5PT7B);
GFont FONT_TAHOMA_REGULAR_FONT6PT7B = GFont(TAHOMA_REGULAR_FONT6PT7B);
GFont FONT_TAHOMA_REGULAR_FONT7PT7B = GFont(TAHOMA_REGULAR_FONT7PT7B);
GFont FONT_TAHOMA_REGULAR_FONT8PT7B = GFont(TAHOMA_REGULAR_FONT8PT7B);
GFont FONT_TAHOMA_REGULAR_FONT9PT7B = GFont(TAHOMA_REGULAR_FONT9PT7B);
GFont FONT_TAHOMA_REGULAR_FONT10PT7B = GFont(TAHOMA_REGULAR_FONT10PT7B);
GFont FONT_TAHOMA_REGULAR_FONT20PT7B = GFont(TAHOMA_REGULAR_FONT10PT7B);
