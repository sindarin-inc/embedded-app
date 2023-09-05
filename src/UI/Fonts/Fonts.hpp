#pragma once

#include "Font.hpp"
#include "FontEntry.hpp"
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontLow.hpp"

// For the IBMF Fonts, a single file contains multiple "face" sizes. A single
// object is created using the IBMFFontLow class that manage the whole .h file
// and "faces" are hooked through the IBMFFont class.

// All the fonts are instanciated in Fonts.cpp

extern std::array<FontEntry, 1> fontsList;

extern IBMFFontLow ibmf75;

extern IBMFFont ibmf75Face0;
extern IBMFFont ibmf75Face1;
extern IBMFFont ibmf75Face2;
extern IBMFFont ibmf75Face3;