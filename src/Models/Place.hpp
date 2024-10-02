#pragma once

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>

#include "DocType.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "sindarin-debug.h"

class Place {
private:
    static constexpr bool REND_ON_ERROR_EXIT = true;

public:
    virtual ~Place() = default;
    [[nodiscard]] virtual auto isFor(DocType docType) const -> bool = 0;
    [[nodiscard]] virtual auto getDocType() const -> DocType = 0;
    [[nodiscard]] virtual auto str() const -> std::string = 0;
    [[nodiscard]] virtual auto isZero() const -> bool = 0;
    [[nodiscard]] inline auto getOffset() const -> uint32_t { return offset_; }
    inline void setOffset(uint32_t offset) { offset_ = offset; }

    static auto factory(const std::string &placeStr, DocType docType = DocType::NONE)
        -> std::shared_ptr<Place>;
    static auto factory(uint32_t offset) -> std::shared_ptr<Place>;
    static auto factory(uint32_t offset, const std::string &href) -> std::shared_ptr<Place>;

    // The following are good for All subclasses BUT the EPubPlace class...

    [[nodiscard]] virtual auto operator==(const Place &rhs) const -> bool {
        return (offset_ == rhs.getOffset());
    }

    [[nodiscard]] virtual auto operator!=(const Place &rhs) const -> bool {
        return (offset_ != rhs.getOffset());
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
    [[nodiscard]] auto isZero() const -> bool override { return true; };
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
    [[nodiscard]] auto isZero() const -> bool override { return offset_ == 0; };
};

// An EPubPlace is the location inside a .epub document. The href_ field relates to the href
// located in the EPub opf's manifest. The offset_ field is a displacement inside the file
// identified by the href_ field.
class EPubPlace : public Place {
public:
    EPubPlace(uint32_t offset, const std::string_view &href) : href_(href) { setOffset(offset); }
    EPubPlace() : Place() { setOffset(0); }
    ~EPubPlace() override = default;

    [[nodiscard]] auto isFor(DocType docType) const -> bool override {
        return docType == DocType::EPUB;
    }
    [[nodiscard]] auto getDocType() const -> DocType override { return DocType::EPUB; };
    [[nodiscard]] auto getOffset() const -> uint32_t { return offset_; }
    [[nodiscard]] auto getHRef() const -> const SpiramString & { return href_; }

    [[nodiscard]] auto str() const -> std::string override {
        std::string s;
        s = "[EPUB;offset:";
        s += std::to_string(offset_);
        s.append(",href:");
        s += href_;
        s.append("]");
        return s;
    }

    [[nodiscard]] auto isZero() const -> bool override { return href_ == "null"; };

    void setHRef(const std::string_view &href) { href_ = href; };

    [[nodiscard]] auto operator==(const EPubPlace &rhs) const -> bool {
        return rhs.isFor(DocType::EPUB) && (offset_ == rhs.getOffset()) && (href_ == rhs.getHRef());
    }

    [[nodiscard]] auto operator==(const Place &rhs) const -> bool override {
        return rhs.isFor(DocType::EPUB) && (*this == reinterpret_cast<const EPubPlace &>(rhs));
    }

private:
    SpiramString href_;
};
