#pragma once

#include <cinttypes>
#include <string>

enum class PlaceType : uint8_t { TXT, EPUB, NONE };

class Place {
public:
    [[nodiscard]] virtual auto isA(PlaceType placeType) const -> bool = 0;
    [[nodiscard]] virtual auto getPlaceType() const -> PlaceType = 0;
    [[nodiscard]] virtual auto str() const -> std::string = 0;
};

class NonePlace : public Place {
public:
    [[nodiscard]] auto isA(PlaceType placeType) const -> bool override {
        return placeType == PlaceType::NONE;
    }
    [[nodiscard]] auto getPlaceType() const -> PlaceType override { return PlaceType::NONE; };
    [[nodiscard]] auto str() const -> std::string override {
        return std::string("[INVALID PLACE]");
    }
};

class TxtPlace : public Place {
public:
    TxtPlace(uint32_t offset) : offset_(offset) {}
    TxtPlace() : offset_(0) {}
    [[nodiscard]] auto isA(PlaceType placeType) const -> bool override {
        return placeType == PlaceType::TXT;
    }
    [[nodiscard]] auto getPlaceType() const -> PlaceType override { return PlaceType::TXT; };
    [[nodiscard]] auto getOffset() const -> uint32_t { return offset_; }

    [[nodiscard]] auto str() const -> std::string override {
        std::string s;
        s = "[offset:";
        s += std::to_string(offset_);
        s.append("]");
        return s;
    }

private:
    uint32_t offset_;
};

class EPubPlace : public Place {
public:
    EPubPlace(uint32_t offset, std::string filename) : offset_(offset), filename_(filename) {}
    EPubPlace() : offset_(0), filename_("") {}

    [[nodiscard]] auto isA(PlaceType placeType) const -> bool override {
        return placeType == PlaceType::EPUB;
    }
    [[nodiscard]] auto getPlaceType() const -> PlaceType override { return PlaceType::EPUB; };
    [[nodiscard]] auto getOffset() const -> uint32_t { return offset_; }
    [[nodiscard]] auto getFilename() const -> const std::string & { return filename_; }

    [[nodiscard]] auto str() const -> std::string override {
        std::string s;
        s = "[offset:";
        s += std::to_string(offset_);
        s.append(", filename:");
        s += filename_;
        s.append("]");
        return s;
    }

private:
    uint32_t offset_;
    std::string filename_;
};
