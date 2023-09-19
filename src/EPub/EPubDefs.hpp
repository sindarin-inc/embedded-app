#pragma once

#include <cinttypes>
#include <utility>

#include "sindarin-debug.h"

using Idx = uint16_t;

#define EPUB_PROFILING 0

#if EPUB_PROFILING
#define EPUB_PROFILE_START(v) PROFILE_START(v)
#define EPUB_PROFILE_END(v) PROFILE_END_LOG_MS(v)
#else
#define EPUB_PROFILE_START(v)
#define EPUB_PROFILE_END(v)
#endif
