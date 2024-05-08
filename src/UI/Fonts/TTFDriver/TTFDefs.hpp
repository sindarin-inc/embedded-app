#include "config.h"

#if TTF_SUPPORT

const constexpr bool TTF_TRACING = false;

#ifndef TTF_TESTING
#define TTF_TESTING 0
#endif

// The following is used when testing the driver outside of this application
#if TTF_TESTING
#define PROGMEM
#include <cstdarg>
#include <cstdio>
#include <string>

extern char *formatStr(const std::string &format, ...);

#define log_i(format, ...) std::cout << "INFO: " << formatStr(format, ##__VA_ARGS__) << std::endl;
#define log_w(format, ...)                                                                         \
    std::cout << "WARNING: " << formatStr(format, ##__VA_ARGS__) << std::endl;
#define log_e(format, ...) std::cout << "ERROR: " << formatStr(format, ##__VA_ARGS__) << std::endl;
#define log_d(format, ...) std::cout << "DEBUG: " << formatStr(format, ##__VA_ARGS__) << std::endl;
#else
#include "sindarin-debug.h"
#endif

namespace ttf_defs {

#include "UI/Fonts/FontDefs.hpp"

typedef uint16_t GlyphCode;
typedef uint8_t *MemoryPtr;

} // namespace ttf_defs

#endif
