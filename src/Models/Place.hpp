#pragma once

#if 0
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
#else
#if 0
class Place {};

class TxtPlace : public Place {
public:
    uint32_t offset;
};

class EPubPlace : public Place {
public:
    uint32_t offset;
    std::string fileName;
};
#else
using Place = uint32_t;
#endif
#endif

#if 0
#pragma pack(push, 1)

struct EPubPlacemark {
    std::string filename;
    uint32_t offset;
};

union Place {
    uint32_t txt_offset;
    EPubPlacemark placemark;
};
#pragma pack(pop)
#endif

#if 0
class Place {};

class TxtPlace : public Place {
  public:
    uint32_t offset;
};

class EPubPlacemark : public Place {
  public:
    uint32_t offset;
    std::string filename;
};
#endif