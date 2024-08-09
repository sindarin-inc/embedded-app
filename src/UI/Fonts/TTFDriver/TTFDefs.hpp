#pragma once

#include "config.h"

#if CONFIG_FONT_TTF

#include "UI/Fonts/FontDefs.hpp"

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

#include "UI/Fonts/FontDefs.hpp"

namespace ttf_defs {

#define LOGI(format, ...) log_i(format, ##__VA_ARGS__)
#define LOGW(format, ...) log_w(format, ##__VA_ARGS__)
#define LOGE(format, ...) log_e(format, ##__VA_ARGS__)
#define LOGD(format, ...) log_d(format, ##__VA_ARGS__)

const constexpr int SCREEN_RES_PER_INCH = CONFIG_TTF_SCREEN_RES_PER_INCH;
const constexpr int SUP_SUB_FONT_DOWNSIZING = 2;

using namespace font_defs;

} // namespace ttf_defs

#endif
