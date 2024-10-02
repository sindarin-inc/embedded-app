#include "Place.hpp"

#include <cstring>
#include <iostream>

#include "Misc/SpiramAllocator.hpp"
#include "sindarin-debug.h"

auto Place::factory(const std::string &placeStr, DocType docType) -> std::shared_ptr<Place> {

    uint32_t offset = 0;
    std::string href;

    const char *s = placeStr.c_str();

    if (*s++ == '[') {
        if (strncmp(s, "TXT;offset:", 11) == 0) {
            s += 11;
            const char *e = s;
            while (*e && (*e != ']')) {
                e++;
            }
            if (*e == ']') {
                offset = strtol(s, nullptr, 10);
                return SpiRamPtr<TxtPlace>(offset);
            }
        } else if (strncmp(s, "EPUB;offset:", 12) == 0) {
            s += 12;
            const char *e = s;
            while (*e && (*e != ',')) {
                e++;
            }
            if (*e++ == ',') {
                offset = strtol(s, nullptr, 10);
                s = e;
                if (strncmp(s, "href:", 5) == 0) {
                    s += 5;
                    e = s;
                    while (*e && (*e != ']')) {
                        e++;
                    }
                    if (*e == ']') {
                        href.assign(s, e - s);
                        return SpiRamPtr<EPubPlace>(offset, href);
                    }
                }
            }
        }
    } else if (placeStr.length() > 0) {
        offset = strtol(placeStr.c_str(), nullptr, 10);
        return SpiRamPtr<TxtPlace>(offset);
    }

    if (docType == DocType::TXT) {
        return SpiRamPtr<TxtPlace>(0);
    } else if (docType == DocType::EPUB) {
        return SpiRamPtr<EPubPlace>(0, "");
    } else {
        log_e("Unable to parse place string properly: \"%s\"", placeStr.c_str());
        if constexpr (REND_ON_ERROR_EXIT) {
            exit(1);
        }
        return SpiRamPtr<NonePlace>();
    }
}

auto Place::factory(uint32_t offset, const std::string &href) -> std::shared_ptr<Place> {
    return SpiRamPtr<EPubPlace>(offset, href);
}

auto Place::factory(uint32_t offset) -> std::shared_ptr<Place> {
    return SpiRamPtr<TxtPlace>(offset);
}
