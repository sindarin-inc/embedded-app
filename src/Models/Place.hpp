#pragma once

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>

#include "DocType.hpp"

class Place {
private:
    static constexpr bool REND_ON_ERROR_EXIT = true;

public:
    virtual ~Place() = default;
    [[nodiscard]] virtual auto isFor(DocType docType) const -> bool = 0;
    [[nodiscard]] virtual auto getDocType() const -> DocType = 0;
    [[nodiscard]] virtual auto str() const -> std::string = 0;
    [[nodiscard]] inline auto getOffset() const -> uint32_t { return offset_; }
    inline void setOffset(uint32_t offset) { offset_ = offset; }

    static auto factory(const std::string &placeStr) -> std::shared_ptr<Place>;
    static auto factory(const uint32_t offset) -> std::shared_ptr<Place>;
    static auto factory(const uint32_t offset, const std::string &href) -> std::shared_ptr<Place>;

    // The following are good for All subclasses BUT the EPubPlace class...

    [[nodiscard]] virtual auto operator==(Place &rhs) const -> bool {
        return (offset_ == rhs.getOffset());
    }

    [[nodiscard]] virtual auto operator!=(Place &rhs) const -> bool {
        return (offset_ != rhs.getOffset());
    }

    [[nodiscard]] virtual auto operator>(Place &rhs) const -> bool {
        return (offset_ > rhs.getOffset());
    }

    [[nodiscard]] virtual auto operator>=(Place &rhs) const -> bool {
        return (offset_ >= rhs.getOffset());
    }

    [[nodiscard]] virtual auto operator<(Place &rhs) const -> bool {
        return (offset_ < rhs.getOffset());
    }

    [[nodiscard]] virtual auto operator<=(Place &rhs) const -> bool {
        return (offset_ <= rhs.getOffset());
    }

protected:
    uint32_t offset_{0};
};

// The use of NonePlace will happen if a bug is present in the code.
class NonePlace : public Place {
public:
    ~NonePlace() override = default;
    [[nodiscard]] auto isFor(DocType docType) const -> bool override {
        return docType == DocType::NONE;
    }
    [[nodiscard]] auto getDocType() const -> DocType override { return DocType::NONE; };
    [[nodiscard]] auto str() const -> std::string override { return "[NONE;INVALID PLACE]"; }
};

// A TxtPlace is the location inside a .txt/.txt.gz document. As the document is represented by a
// single file, the offset_ is a displacement from the beginning of the file.
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

// An EPubPlace is the location inside a .epub document. The href_ field relates to the href
// located in the EPub opf's manifest. The offset_ field is a displacement inside the file
// identified by the href_ field.
//
// WARNING:
// The comparison operators assume that the comparison is done inside the same
// file. If not, they return false.
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

    void setHRef(std::string href) { href_ = std::move(href); };

    [[nodiscard]] auto operator==(Place &rhs) const -> bool override {
        return rhs.isFor(DocType::EPUB) && (offset_ == rhs.getOffset()) &&
               (href_ == reinterpret_cast<EPubPlace &>(rhs).getHRef());
    }

    [[nodiscard]] auto operator!=(Place &rhs) const -> bool override {
        return rhs.isFor(DocType::EPUB) &&
               ((offset_ != rhs.getOffset()) ||
                (href_ != reinterpret_cast<EPubPlace &>(rhs).getHRef()));
    }

    [[nodiscard]] auto operator>(Place &rhs) const -> bool override {
        return rhs.isFor(DocType::EPUB) && (offset_ > rhs.getOffset()) &&
               (href_ == reinterpret_cast<EPubPlace &>(rhs).getHRef());
    }

    [[nodiscard]] auto operator>=(Place &rhs) const -> bool override {
        return rhs.isFor(DocType::EPUB) && (offset_ >= rhs.getOffset()) &&
               (href_ == reinterpret_cast<EPubPlace &>(rhs).getHRef());
    }

    [[nodiscard]] auto operator<(Place &rhs) const -> bool override {
        return rhs.isFor(DocType::EPUB) && (offset_ < rhs.getOffset()) &&
               (href_ == reinterpret_cast<EPubPlace &>(rhs).getHRef());
    }

    [[nodiscard]] auto operator<=(Place &rhs) const -> bool override {
        return rhs.isFor(DocType::EPUB) && (offset_ <= rhs.getOffset()) &&
               (href_ == reinterpret_cast<EPubPlace &>(rhs).getHRef());
    }

private:
    std::string href_;
};
