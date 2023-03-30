
#include "IBMFDriver/IBMFFont.hpp"
#include "IBMFDriver/IBMFFontLow.hpp"
#include "IBMFFonts/Tahoma_75.h"

IBMFFontLow tahoma75 = IBMFFontLow();

IBMFFont fontTahoma75BPI9PT = IBMFFont(tahoma75, TAHOMA_75_IBMF, TAHOMA_75_IBMF_LEN, 0);
IBMFFont fontTahoma75BPI10PT = IBMFFont(tahoma75, TAHOMA_75_IBMF, TAHOMA_75_IBMF_LEN, 1);
IBMFFont fontTahoma75BPI12PT = IBMFFont(tahoma75, TAHOMA_75_IBMF, TAHOMA_75_IBMF_LEN, 2);
IBMFFont fontTahoma75BPI14PT = IBMFFont(tahoma75, TAHOMA_75_IBMF, TAHOMA_75_IBMF_LEN, 3);
