#include "SemVer.hpp"

#include <vector>

auto SemVerCompareVersions(const std::string &version1, const std::string &version2)
    -> SemVerComparison {
    // If either version is empty, it is considered invalid
    if (version1.empty() || version2.empty()) {
        return SemVerComparison::Invalid;
    }
    // If it has non numeric or period characters it's invalid
    if (version1.find_first_not_of("0123456789.") != std::string::npos ||
        version2.find_first_not_of("0123456789.") != std::string::npos) {
        return SemVerComparison::Invalid;
    }

    // Split the version strings into individual parts
    std::vector<int> parts1, parts2;
    size_t pos1 = 0, pos2 = 0;
    while (pos1 < version1.size() || pos2 < version2.size()) {
        size_t end1 = version1.find_first_of('.', pos1);
        size_t end2 = version2.find_first_of('.', pos2);
        if (end1 == std::string::npos) {
            end1 = version1.size();
        }
        if (end2 == std::string::npos) {
            end2 = version2.size();
        }
        parts1.push_back(std::stoi(version1.substr(pos1, end1 - pos1)));
        parts2.push_back(std::stoi(version2.substr(pos2, end2 - pos2)));
        pos1 = end1 + 1;
        pos2 = end2 + 1;
    }

    // Compare each part of the versions
    for (size_t i = 0; i < std::min(parts1.size(), parts2.size()); i++) {
        if (parts1[i] < parts2[i]) {
            return SemVerComparison::LessThan;
        } else if (parts1[i] > parts2[i]) {
            return SemVerComparison::GreaterThan;
        }
    }

    // If all parts are equal, but one version has more parts, it is considered greater
    if (parts1.size() < parts2.size()) {
        return SemVerComparison::LessThan;
    } else if (parts1.size() > parts2.size()) {
        return SemVerComparison::GreaterThan;
    } else {
        return SemVerComparison::EqualTo;
    }
}
