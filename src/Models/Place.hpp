#pragma once

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>

enum class PlaceType : uint8_t { TXT, EPUB, NONE };

#define PLACE_REGEX "\\[(TXT|EPUB);offset:([0-9]+)(,filename:([^\\]]*))?\\]"

class Place {
private:
    static constexpr bool ON_ERROR_EXIT = true;

public:
    virtual ~Place() = default;
    [[nodiscard]] virtual auto isA(PlaceType placeType) const -> bool = 0;
    [[nodiscard]] virtual auto getPlaceType() const -> PlaceType = 0;
    [[nodiscard]] virtual auto str() const -> std::string = 0;
    [[nodiscard]] inline auto getOffset() const -> uint32_t { return offset_; }
    inline void setOffset(uint32_t offset) { offset_ = offset; }

    static auto factory(const std::string &placeStr) -> std::shared_ptr<Place>;

protected:
    uint32_t offset_;
};

class NonePlace : public Place {
public:
    ~NonePlace() override = default;
    [[nodiscard]] auto isA(PlaceType placeType) const -> bool override {
        return placeType == PlaceType::NONE;
    }
    [[nodiscard]] auto getPlaceType() const -> PlaceType override { return PlaceType::NONE; };
    [[nodiscard]] auto str() const -> std::string override { return "[NONE;INVALID PLACE]"; }
};

class TxtPlace : public Place {
public:
    TxtPlace(uint32_t offset) { setOffset(offset); }
    TxtPlace() : Place() { setOffset(0); }
    ~TxtPlace() override = default;
    [[nodiscard]] auto isA(PlaceType placeType) const -> bool override {
        return placeType == PlaceType::TXT;
    }
    [[nodiscard]] auto getPlaceType() const -> PlaceType override { return PlaceType::TXT; };
    [[nodiscard]] auto getOffset() const -> uint32_t { return offset_; }

    [[nodiscard]] auto str() const -> std::string override {
        std::string s;
        s = "[TXT;offset:";
        s += std::to_string(offset_);
        s.append("]");
        return s;
    }
};

class EPubPlace : public Place {
public:
    EPubPlace(uint32_t offset, std::string filename) : filename_(std::move(std::move(filename))) {
        setOffset(offset);
    }
    EPubPlace() : Place(), filename_("") { setOffset(0); }
    ~EPubPlace() override = default;

    [[nodiscard]] auto isA(PlaceType placeType) const -> bool override {
        return placeType == PlaceType::EPUB;
    }
    [[nodiscard]] auto getPlaceType() const -> PlaceType override { return PlaceType::EPUB; };
    [[nodiscard]] auto getOffset() const -> uint32_t { return offset_; }
    [[nodiscard]] auto getFilename() const -> const std::string & { return filename_; }

    [[nodiscard]] auto str() const -> std::string override {
        std::string s;
        s = "[EPUB;offset:";
        s += std::to_string(offset_);
        s.append(",filename:");
        s += filename_;
        s.append("]");
        return s;
    }

private:
    std::string filename_;
};
