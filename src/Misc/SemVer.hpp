#include <string>

enum class SemVerComparison { LessThan, EqualTo, GreaterThan, Invalid };

SemVerComparison SemVerCompareVersions(const std::string &version1, const std::string &version2);
