#pragma once

#include <Spiram/String.hpp>
#include <Spiram/Vector.hpp>
#include <string>

#include "Models/JsonSerializable.hpp"
#include "Models/ReadableDictionary.hpp"

struct ReadableTableOfContentsChapter {
    SpiramString title;
    SpiramString href;
    uint32_t epubOffset{};
    uint32_t endEpubOffset{};
    SpiramString inSubsectionHref;
};
