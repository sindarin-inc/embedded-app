#pragma once

#include <cinttypes>

#include "Font.h"
//---
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
//---
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontLow.hpp"

const GFont FONT_LUCIDA_GRANDE5PT7B = GFont(LUCIDA_GRANDE5PT7B);
const GFont FONT_LUCIDA_GRANDE5PT7B_SKINNY = GFont(LUCIDA_GRANDE5PT7B_SKINNY);
const GFont FONT_LUCIDA_GRANDE5PT7B_FAT = GFont(LUCIDA_GRANDE5PT7B_FAT);
const GFont FONT_LUCIDA_GRANDE6PT7B = GFont(LUCIDA_GRANDE6PT7B);
const GFont FONT_LUCIDA_GRANDE10PT7B = GFont(LUCIDA_GRANDE10PT7B);
const GFont FONT_LUCIDA_GRANDE14PT7B = GFont(LUCIDA_GRANDE14PT7B);

const GFont FONT_TAHOMA_REGULAR_FONT5PT7B = GFont(TAHOMA_REGULAR_FONT5PT7B);
const GFont FONT_TAHOMA_REGULAR_FONT6PT7B = GFont(TAHOMA_REGULAR_FONT6PT7B);
const GFont FONT_TAHOMA_REGULAR_FONT7PT7B = GFont(TAHOMA_REGULAR_FONT7PT7B);
const GFont FONT_TAHOMA_REGULAR_FONT8PT7B = GFont(TAHOMA_REGULAR_FONT8PT7B);
const GFont FONT_TAHOMA_REGULAR_FONT9PT7B = GFont(TAHOMA_REGULAR_FONT9PT7B);
const GFont FONT_TAHOMA_REGULAR_FONT10PT7B = GFont(TAHOMA_REGULAR_FONT10PT7B);
const GFont FONT_TAHOMA_REGULAR_FONT20PT7B = GFont(TAHOMA_REGULAR_FONT10PT7B);

// For the IBMF Fonts, a single file contains multiple "face" sizes. A single
// object is created using the IBMFFontLow class that manage the whole .h file
// and "faces" are hooked through the IBMFFont class.

// extern IBMFFontLow EC_Regular_75; // Instanciated in Fonts.cpp

// extern IBMFFont FONT_EC_REGULAR_75BPI12PT; // Instanciated in Fonts.cpp
// extern IBMFFont FONT_EC_REGULAR_75BPI14PT; // Instanciated in Fonts.cpp
// extern IBMFFont FONT_EC_REGULAR_75BPI17PT; // Instanciated in Fonts.cpp

/// extern IBMFFontLow ECSans_Regular_75; // Instanciated in Fonts.cpp

// extern IBMFFont FONT_ECSANS_REGULAR_75BPI12PT; // Instanciated in Fonts.cpp
/// extern IBMFFont FONT_ECSANS_REGULAR_75BPI14PT; // Instanciated in Fonts.cpp
// extern IBMFFont FONTSANS_EC_REGULAR_75BPI17PT; // Instanciated in Fonts.cpp

// extern IBMFFontLow EC_Regular_100; // Instanciated in Fonts.cpp

// extern IBMFFont FONT_EC_REGULAR_100BPI12PT; // Instanciated in Fonts.cpp
// extern IBMFFont FONT_EC_REGULAR_100BPI14PT; // Instanciated in Fonts.cpp
// extern IBMFFont FONT_EC_REGULAR_100BPI17PT; // Instanciated in Fonts.cpp

// extern IBMFFontLow ECSans_Regular_100; // Instanciated in Fonts.cpp

// extern IBMFFont FONT_ECSANS_REGULAR_100BPI12PT; // Instanciated in Fonts.cpp
// extern IBMFFont FONT_ECSANS_REGULAR_100BPI14PT; // Instanciated in Fonts.cpp
// extern IBMFFont FONT_ECSANS_REGULAR_100BPI17PT; // Instanciated in Fonts.cpp