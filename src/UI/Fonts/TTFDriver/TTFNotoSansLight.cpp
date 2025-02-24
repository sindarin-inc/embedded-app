#if CONFIG_FONT_TTF

#include "TTFNotoSansLight.hpp"

#include <optional>

template <typename T, typename U, std::size_t N>
auto BinarySearch(const std::array<std::pair<T, U>, N> &arr, const T &value) -> std::optional<U> {
    int left = 0;
    int right = N - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (arr[mid].first == value) {
            return arr[mid].second; // Found the value, return the second item
        }
        if (arr[mid].first < value) {
            left = mid + 1; // Continue search on the right half
        } else {
            right = mid - 1; // Continue search on the left half
        }
    }

    return std::nullopt; // Value not found
}

auto TTFNotoSansLight::ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const
    -> bool {

    *kern = 0;

    // If there is a ligature defined, set it in glyphCode2 and return true such that
    // this method will be called again. If no ligature continue below for kening checks.

    uint32_t key = (glyphCode1 << 16) + *glyphCode2;

    auto resLigs = BinarySearch(notoSansLight.ligatures_, key);

    if (resLigs.has_value()) {
        // LOGW("====> Ligature found: (%d, %d) -> %d.", glyphCode1, *glyphCode2, resLigs.value());
        *glyphCode2 = resLigs.value();
        return true;
    }

    // No ligature, try to find a kerning value in the class-based structs.

    auto resDef1 = BinarySearch(notoSansLight.classesDefs_, glyphCode1);
    auto resDef2 = BinarySearch(notoSansLight.classesDefs_, *glyphCode2);

    // A class Id == 99 means undefined.

    if (resDef1.has_value() && resDef2.has_value() && (resDef1.value().first != 99) &&
        (resDef2.value().second != 99)) {
        auto value = notoSansLight.mKerns_[resDef1.value().first][resDef2.value().second];
        if (value != 0) {
            *kern = value;
            // LOGW("Class kern for %d (%d) and %d (%d): %d (%d)", glyphCode1,
            // resDef1.value().first,
            //      *glyphCode2, resDef2.value().second, value, *kern);
        }
        return false;
    }

    // No kerning in the class-based structs, check for one in the kerning pairs struct.

    auto res2 = BinarySearch(notoSansLight.kerns_, key);

    if (res2.has_value()) {
        *kern = res2.value();
        // LOGW("Kern value: %d -> %d", res2.value(), *kern);
    }

    return false;
}

#endif