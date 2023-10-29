#include <cinttypes>
#include <string>

#include "IBMFDefs.hpp"

// A very simple UTF8 iterator.
//
// Added verification to insure that the next UTF8 character is well formed
//
// Bytes format:
//
// Byte count  | bytes representation (bits)         | code value (bits)
// ------------+-------------------------------------+------------------------
// One byte    | 0xxxxxxx                            | xxxxxxx
// Two Bytes   | 110aaaaa 10bbbbbb                   | aaaaabbbbbb
// Three Bytes | 1110aaaa 10bbbbbb 10cccccc          | aaaabbbbbbcccccc
// Four Bytes  | 11110aaa 10bbbbbb 10cccccc 10dddddd | aaabbbbbbccccccdddddd

class UTF8Iterator {

private:
    std::string::const_iterator stringIterator_;
    const std::string &string_;

    static constexpr unsigned char MASK_1 = 0x80;
    static constexpr unsigned char MASK_2 = 0x40;
    static constexpr unsigned char MASK_3 = 0x20;
    static constexpr unsigned char MASK_4 = 0x10;

public:
    UTF8Iterator(const std::string &str) : string_(str) { stringIterator_ = str.begin(); }

    ~UTF8Iterator() = default;

    auto operator++() -> UTF8Iterator & {
        if (stringIterator_ != string_.end()) {
            char firstByte = *stringIterator_;

            std::string::difference_type offset = 1;

            if (firstByte & MASK_1) {
                // The first byte has a value greater than 0x7F, so not ASCII
                if (firstByte & MASK_2) {
                    if (firstByte & MASK_3) {
                        // The first byte has a value greater or equal to
                        // 0xE0, and so it must be at least a three-octet code point.
                        if (firstByte & MASK_4) {
                            // The first byte has a value greater or equal to
                            // 0xF0, and so it must be a four-octet code point.
                            offset = 4;
                        } else {
                            offset = 3;
                        }
                    } else {
                        offset = 2;
                    }
                } else {
                    // First byte is not larger or equal to 0xC0. It must be a malformed
                    // UTF8 character. Bypass the bytes that may be part of the malformed
                    // character.
                    auto temp = stringIterator_;
                    offset = 0;
                    while ((temp != string_.end()) && ((*temp & 0xC0) == 0x80)) {
                        offset++;
                        temp++;
                    }
                }
            }

            bool first = true;
            while ((offset > 0) && (stringIterator_ != string_.end())) {
                if (!first && ((*stringIterator_ & 0xC0) != 0x80)) {
                    break;
                }
                first = false;
                offset--;
                stringIterator_++;
            }
        }
        return *this;
    }

    auto operator++(int) -> UTF8Iterator {
        UTF8Iterator temp = *this;
        ++(*this);
        return temp;
    }

    auto operator--() -> UTF8Iterator & {
        if (stringIterator_ != string_.begin()) {
            --stringIterator_;

            if ((stringIterator_ != string_.begin()) && (*stringIterator_ & MASK_1)) {
                // The previous byte is not an ASCII character.
                --stringIterator_;
                if ((stringIterator_ != string_.begin()) && ((*stringIterator_ & MASK_2) == 0)) {
                    --stringIterator_;
                    if ((stringIterator_ != string_.begin()) &&
                        ((*stringIterator_ & MASK_3) == 0)) {
                        --stringIterator_;
                    }
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
        char32_t chr = ibmf_defs::UNKNOWN_CODEPOINT;

        if (stringIterator_ != string_.end()) {
            char byte1 = *stringIterator_;
            auto remainderSize = string_.end() - stringIterator_;

            if (byte1 & MASK_1) {
                // The first byte has a value greater than 0x7F, and so is beyond the ASCII range.
                if (byte1 & MASK_2) {
                    if (remainderSize > 1) {
                        char byte2 = *(stringIterator_ + 1);
                        if ((byte2 & 0xC0) == 0x80) {
                            if (byte1 & MASK_3) {
                                if (remainderSize > 2) {
                                    // The first byte has a value greater than
                                    // 0xCF, and so it must be at least a three-octet code point.
                                    char byte3 = *(stringIterator_ + 2);
                                    if ((byte3 & 0xC0) == 0x80) {
                                        if (byte1 & MASK_4) {
                                            if (remainderSize > 3) {
                                                // The first byte has a value greater than
                                                // 224, and so it must be a four-octet code point.
                                                char byte4 = *(stringIterator_ + 3);
                                                if ((byte4 & 0xC0) == 0x80) {
                                                    chr = ((byte1 & 0x07) << 18) +
                                                          ((byte2 & 0x3f) << 12) +
                                                          ((byte3 & 0x3f) << 6) + (byte4 & 0x3f);
                                                }
                                            }
                                        } else {
                                            chr = ((byte1 & 0x0f) << 12) + ((byte2 & 0x3f) << 6) +
                                                  (byte3 & 0x3f);
                                        }
                                    }
                                }
                            } else {
                                chr = ((byte1 & 0x1f) << 6) + (byte2 & 0x3f);
                            }
                        }
                    }
                }
            } else {
                chr = byte1;
            }
        }

        return chr;
    }
};
