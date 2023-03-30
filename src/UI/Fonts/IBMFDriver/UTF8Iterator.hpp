#include <cinttypes>
#include <string>

// A very simple UTF8 iterator.

class UTF8Iterator {

private:
    std::string::const_iterator stringIterator_;

    static constexpr unsigned char MASK_1 = 0x80;
    static constexpr unsigned char MASK_2 = 0x40;
    static constexpr unsigned char MASK_3 = 0x20;
    static constexpr unsigned char MASK_4 = 0x10;
    static constexpr unsigned char MASK_5 = 0x08;

public:
    UTF8Iterator(std::string::const_iterator it) { stringIterator_ = it; }

    UTF8Iterator(const UTF8Iterator &source) { stringIterator_ = source.stringIterator_; }

    auto operator=(const UTF8Iterator &rhs) -> UTF8Iterator & {
        if (this != &rhs) {
            stringIterator_ = rhs.stringIterator_;
        }
        return *this;
    }

    ~UTF8Iterator() = default;

    auto operator++() -> UTF8Iterator & {
        char firstByte = *stringIterator_;

        std::string::difference_type offset = 1;

        if (firstByte & MASK_1) {
            // The first byte has a value greater than 127, so not ASCII
            if (firstByte & MASK_3) {
                // The first byte has a value greater than
                // 224, and so it must be at least a three-octet code point.
                if (firstByte & MASK_4) {
                    // The first byte has a value greater than
                    // 240, and so it must be a four-octet code point.
                    offset = 4;
                } else {
                    offset = 3;
                }
            } else {
                offset = 2;
            }
        }

        stringIterator_ += offset;

        return *this;
    }

    auto operator++(int) -> UTF8Iterator {
        UTF8Iterator temp = *this;
        ++(*this);
        return temp;
    }

    auto operator--() -> UTF8Iterator & {
        --stringIterator_;

        if (*stringIterator_ & MASK_1) {
            // The previous byte is not an ASCII character.
            --stringIterator_;
            if ((*stringIterator_ & MASK_2) == 0) {
                --stringIterator_;
                if ((*stringIterator_ & MASK_3) == 0) {
                    --stringIterator_;
                }
            }
        }

        return *this;
    }

    auto operator--(int) -> UTF8Iterator {
        UTF8Iterator temp = *this;
        --(*this);
        return temp;
    }

    auto operator==(const UTF8Iterator &rhs) const -> bool {
        return stringIterator_ == rhs.stringIterator_;
    }

    auto operator!=(const UTF8Iterator &rhs) const -> bool {
        return stringIterator_ != rhs.stringIterator_;
    }

    auto operator==(std::string::iterator rhs) const -> bool { return stringIterator_ == rhs; }

    auto operator==(std::string::const_iterator rhs) const -> bool {
        return stringIterator_ == rhs;
    }

    auto operator!=(std::string::iterator rhs) const -> bool { return stringIterator_ != rhs; }

    auto operator!=(std::string::const_iterator rhs) const -> bool {
        return stringIterator_ != rhs;
    }

    auto operator*() const -> char32_t {
        char32_t chr = 0;

        char firstByte = *stringIterator_;

        if (firstByte & MASK_1) {
            // The first byte has a value greater than 127, and so is beyond the ASCII range.
            if (firstByte & MASK_3) {
                // The first byte has a value greater than
                // 191, and so it must be at least a three-octet code point.
                if (firstByte & MASK_4) {
                    // The first byte has a value greater than
                    // 224, and so it must be a four-octet code point.
                    chr = ((firstByte & 0x07) << 18) + ((*(stringIterator_ + 1) & 0x3f) << 12) +
                          ((*(stringIterator_ + 2) & 0x3f) << 6) + (*(stringIterator_ + 3) & 0x3f);
                } else {
                    chr = ((firstByte & 0x0f) << 12) + ((*(stringIterator_ + 1) & 0x3f) << 6) +
                          (*(stringIterator_ + 2) & 0x3f);
                }
            } else {
                chr = ((firstByte & 0x1f) << 6) + (*(stringIterator_ + 1) & 0x3f);
            }
        } else {
            chr = firstByte;
        }

        return chr;
    }
};
