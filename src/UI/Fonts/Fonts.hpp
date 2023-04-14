#pragma once

#include "Font.hpp"
#include "FontEntry.hpp"
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontLow.hpp"

// For the IBMF Fonts, a single file contains multiple "face" sizes. A single
// object is created using the IBMFFontLow class that manage the whole .h file
// and "faces" are hooked through the IBMFFont class.

// All the fonts are instanciated in Fonts.cpp

extern IBMFFontLow ibmf75;

extern IBMFFont ibmf75Face0;
extern IBMFFont ibmf75Face1;
extern IBMFFont ibmf75Face2;
extern IBMFFont ibmf75Face3;

extern std::array<FontEntry, 18> fontsList;

extern GFont fontLucidaGrandE5PT7B;
extern GFont fontLucidaGrandE5PT7BSkinny;
extern GFont fontLucidaGrandE5PT7BFat;
extern GFont fontLucidaGrandE6PT7B;
extern GFont fontLucidaGrandE10PT7B;
extern GFont fontLucidaGrandE14PT7B;
extern GFont fontTahomaRegularFonT5PT7B;
extern GFont fontTahomaRegularFonT6PT7B;
extern GFont fontTahomaRegularFonT7PT7B;
extern GFont fontTahomaRegularFonT8PT7B;
extern GFont fontTahomaRegularFonT9PT7B;
extern GFont fontTahomaRegularFonT10PT7B;
extern GFont fontTahomaRegularFonT20PT7B;