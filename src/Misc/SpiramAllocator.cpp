#include "SpiramAllocator.hpp"

auto operator==(const std::string &stdStr, const SpiramString &spiramStr) -> bool {
    return spiramStr.compare(stdStr.c_str()) == 0;
}
