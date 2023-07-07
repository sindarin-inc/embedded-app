#pragma once

#include <cinttypes>
#include <string>

#pragma pack(push, 1)
struct Place {
    uint32_t offset;
    uint16_t fileNumber;

    auto operator==(const Place &rhs) const -> bool {
        return (offset == rhs.offset) && (fileNumber == rhs.fileNumber);
    }

    auto str() const -> std::string {
        std::string s;
        s = "[offset:";
        s += std::to_string(offset);
        s.append(" ,fileNbr:");
        s += std::to_string(fileNumber);
        s.append("]");
        return s;
    }
};
#pragma pack(pop)
