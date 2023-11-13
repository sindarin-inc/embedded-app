#include "Place.hpp"

#include <cstring>
#include <iostream>

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
                return std::make_shared<TxtPlace>(offset);
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
                        return std::make_shared<EPubPlace>(offset, href);
                    }
                }
            }
        }
    } else if (placeStr.length() > 0) {
        offset = strtol(placeStr.c_str(), nullptr, 10);
        return std::make_shared<TxtPlace>(offset);
    }

    if (docType == DocType::TXT) {
        return std::make_shared<TxtPlace>(0);
    } else if (docType == DocType::EPUB) {
        return std::make_shared<EPubPlace>(0, "");
    } else {
        log_e("Unable to parse place string properly: \"%s\"", placeStr.c_str());
        if constexpr (REND_ON_ERROR_EXIT) {
            exit(1);
        }
        return std::make_shared<NonePlace>();
    }
}

auto Place::factory(const uint32_t offset, const std::string &href) -> std::shared_ptr<Place> {
    return std::make_shared<EPubPlace>(offset, href);
}

auto Place::factory(const uint32_t offset) -> std::shared_ptr<Place> {
    return std::make_shared<TxtPlace>(offset);
}
