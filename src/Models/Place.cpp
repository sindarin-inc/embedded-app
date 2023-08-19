#include "Place.hpp"

#include <cstring>
#include <iostream>
#include <regex>

#include "sindarin-debug.h"

auto Place::factory(const std::string &placeStr) -> std::shared_ptr<Place> {
    std::regex regExStr("^" PLACE_REGEX "$");
    std::smatch sm;
    if (regex_match(placeStr, sm, regExStr) && (sm.size() == 5)) {
        std::cout << sm[1] << std::endl;
        if (sm[1] == "TXT") {
            uint32_t offset = atoi(sm[2].str().c_str());
            return std::make_shared<TxtPlace>(offset);

        } else if (sm[1] == "EPUB") {
            uint32_t offset = atoi(sm[2].str().c_str());
            std::string filename(sm[4]);
            return std::make_shared<EPubPlace>(offset, filename);
        }
    }

    uint32_t offset = atoi(placeStr.c_str());
    return std::make_shared<TxtPlace>(offset);

    // log_e("Unable to parse place string properly: \"%s\"", placeStr.c_str());
    // if constexpr (ON_ERROR_EXIT) {
    //     exit(1);
    // }
    // return std::make_shared<NonePlace>();
};
