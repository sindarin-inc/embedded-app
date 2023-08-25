#pragma once

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>

#include "DocType.hpp"

class Place {
private:
    static constexpr bool ON_ERROR_EXIT = true;

public:
    virtual ~Place() = default;
    [[nodiscard]] virtual auto isFor(DocType docType) const -> bool = 0;
    [[nodiscard]] virtual auto getDocType() const -> DocType = 0;
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
    [[nodiscard]] auto isFor(DocType docType) const -> bool override {
        return docType == DocType::NONE;
    }
    [[nodiscard]] auto getDocType() const -> DocType override { return DocType::NONE; };
    [[nodiscard]] auto str() const -> std::string override { return "[NONE;INVALID PLACE]"; }
};

class TxtPlace : public Place {
public:
    TxtPlace(uint32_t offset) { setOffset(offset); }
    TxtPlace() : Place() { setOffset(0); }
    ~TxtPlace() override = default;
    [[nodiscard]] auto isFor(DocType docType) const -> bool override {
        return docType == DocType::TXT;
    }
    [[nodiscard]] auto getDocType() const -> DocType override { return DocType::TXT; };
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
    EPubPlace(uint32_t offset, std::string href) : href_(std::move(std::move(href))) {
        setOffset(offset);
    }
    EPubPlace() : Place(), href_("") { setOffset(0); }
    ~EPubPlace() override = default;

    [[nodiscard]] auto isFor(DocType docType) const -> bool override {
        return docType == DocType::EPUB;
    }
    [[nodiscard]] auto getDocType() const -> DocType override { return DocType::EPUB; };
    [[nodiscard]] auto getOffset() const -> uint32_t { return offset_; }
    [[nodiscard]] auto getHRef() const -> const std::string & { return href_; }

    [[nodiscard]] auto str() const -> std::string override {
        std::string s;
        s = "[EPUB;offset:";
        s += std::to_string(offset_);
        s.append(",href:");
        s += href_;
        s.append("]");
        return s;
    }

private:
    std::string href_;
};
