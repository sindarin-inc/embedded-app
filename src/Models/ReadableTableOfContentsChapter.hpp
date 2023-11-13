#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/ReadableDictionary.hpp"

struct ReadableTableOfContentsChapter {
    SpiramString title;
    SpiramString href;
    uint32_t epubOffset;
    uint32_t endEpubOffset;
    SpiramString inSubsectionHref;
};
