#pragma once

#include "Font.hpp"
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontLow.hpp"

// For the IBMF Fonts, a single file contains multiple "face" sizes. A single
// object is created using the IBMFFontLow class that manage the whole .h file
// and "faces" are hooked through the IBMFFont class.

extern IBMFFontLow tahoma75; // Instanciated in Fonts.cpp

extern IBMFFont fontTahoma75BPI9PT;  // Instanciated in Fonts.cpp
extern IBMFFont fontTahoma75BPI10PT; // Instanciated in Fonts.cpp
extern IBMFFont fontTahoma75BPI12PT; // Instanciated in Fonts.cpp
extern IBMFFont fontTahoma75BPI14PT; // Instanciated in Fonts.cpp

extern GFont FONT_LUCIDA_GRANDE5PT7B;
extern GFont FONT_LUCIDA_GRANDE5PT7B_SKINNY;
extern GFont FONT_LUCIDA_GRANDE5PT7B_FAT;
extern GFont FONT_LUCIDA_GRANDE6PT7B;
extern GFont FONT_LUCIDA_GRANDE10PT7B;
extern GFont FONT_LUCIDA_GRANDE14PT7B;
extern GFont FONT_TAHOMA_REGULAR_FONT5PT7B;
extern GFont FONT_TAHOMA_REGULAR_FONT6PT7B;
extern GFont FONT_TAHOMA_REGULAR_FONT7PT7B;
extern GFont FONT_TAHOMA_REGULAR_FONT8PT7B;
extern GFont FONT_TAHOMA_REGULAR_FONT9PT7B;
extern GFont FONT_TAHOMA_REGULAR_FONT10PT7B;
extern GFont FONT_TAHOMA_REGULAR_FONT20PT7B;