#include <string>

enum class SemVerComparison { LessThan, EqualTo, GreaterThan, Invalid };

auto SemVerCompareVersions(const std::string &version1, const std::string &version2)
    -> SemVerComparison;
