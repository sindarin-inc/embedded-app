#pragma once

#if SIMULATOR
#define AppVersion "SIMULATOR"
#else
#include <esp_app_desc.h>
#define AppVersion (esp_app_get_description()->version)
#endif
