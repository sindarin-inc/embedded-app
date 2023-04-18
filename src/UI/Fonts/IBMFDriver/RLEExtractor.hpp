#pragma once

#include <cinttypes>
#include <cstring>
#include <iostream>

#include "IBMFDefs.hpp"

using namespace ibmf_defs;

class RLEExtractor {
private:
    static constexpr char const *TAG = "RLEExtractor";
    MemoryPtr fromPixelsPtr_, fromPixelsEnd_;

    uint32_t repeatCount_;

    uint8_t nybbleFlipper_ = 0xf0U;
    uint8_t nybbleByte_;

    PixelResolution resolution_;

    static constexpr uint8_t PK_REPEAT_COUNT = 14;
    static constexpr uint8_t PK_REPEAT_ONCE = 15;

    auto getnext8(uint8_t &val) -> bool {
        if (fromPixelsPtr_ >= fromPixelsEnd_) {
            return false;
        }
        val = *fromPixelsPtr_++;
        return true;
    }

    auto getNybble(uint8_t &nyb) -> bool {
        if (nybbleFlipper_ == 0xf0U) {
            if (!getnext8(nybbleByte_)) {
                return false;
            }
            nyb = nybbleByte_ >> 4;
        } else {
            nyb = (nybbleByte_ & 0x0f);
        }
        nybbleFlipper_ ^= 0xffU;
        return true;
    }

    // Pseudo-code:
    //
    // function pkPackedNum: integer;
    // var i,j,k: integer;
    // begin
    //   i := get_nyb;
    //   if i = 0 then begin
    //     repeat
    //       j := getnyb; incr(i);
    //     until j != 0;
    //     while i > 0 do begin
    //       j := j * 16 + get_nyb;
    //       decr(i);
    //     end;
    //     pkPackedNum := j - 15 + (13 - dynF) * 16 + dynF;
    //   end
    //   else if i <= dynF then
    //     pkPackedNum := i
    //   else if i < 14 then
    //     pkPackedNum := (i - dynF - 1) * 16 + get_nyb + dynF + 1
    //   else begin
    //     if repeatCount_ != 0 then abort('Extra repeat count!');
    //     if i = 14 then
    //        repeatCount_ := pkPackedNum
    //     else
    //        repeatCount_ := 1;
    //     send_out(true, repeatCount_);
    //     pkPackedNum := pkPackedNum;
    //   end;
    // end;

    auto getPackedNumber(uint32_t &val, const RLEMetrics &rleMetrics) -> bool {
        uint8_t nyb;
        uint32_t i, j;

        uint8_t dynF = rleMetrics.dynF;

        while (true) {
            if (!getNybble(nyb)) {
                return false;
            }
            i = nyb;
            if (i == 0) {
                do {
                    if (!getNybble(nyb)) {
                        return false;
                    }
                    i++;
                } while (nyb == 0);
                j = nyb;
                while (i-- > 0) {
                    if (!getNybble(nyb)) {
                        return false;
                    }
                    j = (j << 4) + nyb;
                }
                val = j - 15 + ((13 - dynF) << 4) + dynF;
                break;
            } else if (i <= dynF) {
                val = i;
                break;
            } else if (i < PK_REPEAT_COUNT) {
                if (!getNybble(nyb)) {
                    return false;
                }
                val = ((i - dynF - 1) << 4) + nyb + dynF + 1;
                break;
            } else {
                // if (repeatCount_ != 0) {
                //   std::cerr << "Spurious repeatCount iteration!" << std::endl;
                //   return false;
                // }
                if (i == PK_REPEAT_COUNT) {
                    if (!getPackedNumber(repeatCount_, rleMetrics)) {
                        return false;
                    }
                } else { // i == PK_REPEAT_ONCE
                    repeatCount_ = 1;
                }
            }
        }
        return true;
    }

    inline auto copyOneRowEightBits(MemoryPtr fromLine, MemoryPtr toLine, int16_t fromCol,
                                    int size) const -> void {
        memcpy(toLine + fromCol, fromLine + fromCol, size);
    }

    auto copyOneRowOneBit(MemoryPtr fromLine, MemoryPtr toLine, int16_t fromCol, int16_t endCol,
                          bool inverted) const -> void {
        uint8_t mask = 0x80 >> (fromCol & 7);
        int idx = fromCol >> 3;
        for (int i = fromCol; i < endCol; i++) {
            if constexpr (BLACK_ONE_BIT) {
                if (inverted) {
                    toLine[idx] &= fromLine[idx] | ~mask;
                } else {
                    toLine[idx] |= (fromLine[idx] & mask);
                }
            } else {
                if (inverted) {
                    toLine[idx] |= (fromLine[idx] & mask);
                } else {
                    toLine[idx] &= fromLine[idx] | ~mask;
                }
            }
            mask >>= 1;
            if (mask == 0) {
                mask = 0x80;
                idx += 1;
            }
        }
    }

public:
    RLEExtractor(PixelResolution resolution) : resolution_(resolution) {}

    auto retrieveBitmap(const RLEBitmap &fromBitmap, Bitmap &toBitmap, const Pos atOffset,
                        const RLEMetrics rleMetrics, bool inverted) -> bool {
        // point on the glyphs' bitmap definition
        fromPixelsPtr_ = fromBitmap.pixels;
        fromPixelsEnd_ = fromPixelsPtr_ + fromBitmap.length;
        MemoryPtr toRowPtr;

        if ((atOffset.x < 0) || (atOffset.y < 0) ||
            ((atOffset.y + fromBitmap.dim.height) > toBitmap.dim.height) ||
            ((atOffset.x + fromBitmap.dim.width) > toBitmap.dim.width)) {
            return false;
        }

        if (resolution_ == PixelResolution::ONE_BIT) {
            uint32_t toRowSize = (toBitmap.dim.width + 7) >> 3;
            toRowPtr = toBitmap.pixels + static_cast<size_t>(atOffset.y * toRowSize);

            if (rleMetrics.dynF == 14) { // is a non-compressed RLE?
                uint8_t data;
                uint8_t fromMask = 0;

                for (uint32_t fromRow = 0; fromRow < fromBitmap.dim.height;
                     fromRow++, toRowPtr += toRowSize) {
                    uint8_t toMask = 0x80 >> (atOffset.x & 7);
                    int idx = (atOffset.x >> 3);
                    for (uint32_t i = 0; i < fromBitmap.dim.width; i++) {
                        if (fromMask == 0) {
                            fromMask = 0x80;
                            if (!getnext8(data)) {
                                std::cerr << "Not enough bitmap data!" << std::endl;
                                return false;
                            }
                            // std::cout << std::hex << +data << ' ';
                            // count = 0;
                        }
                        if (data & fromMask) {
                            if constexpr (BLACK_ONE_BIT) {
                                if (inverted) {
                                    toRowPtr[idx] &= ~toMask;
                                } else {
                                    toRowPtr[idx] |= toMask;
                                }

                            } else {
                                if (inverted) {
                                    toRowPtr[idx] |= toMask;
                                } else {
                                    toRowPtr[idx] &= ~toMask;
                                }
                            }
                        }
                        fromMask >>= 1;
                        toMask >>= 1;
                        if (toMask == 0) {
                            idx += 1;
                            toMask = 0x80;
                        }
                        // count++;
                    }
                }
                // std::cout << std::endl;
            } else {
                uint32_t count = 0;

                repeatCount_ = 0;
                nybbleFlipper_ = 0xf0U;

                // the variable will be reversed at first execution of the inner loop
                bool black = !rleMetrics.firstIsBlack;

                for (uint32_t fromRow = 0; fromRow < fromBitmap.dim.height;
                     fromRow++, toRowPtr += toRowSize) {
                    uint8_t toMask = 0x80 >> (atOffset.x & 7);
                    int idx = (atOffset.x >> 3);
                    int firstBlack = -1;
                    for (uint32_t i = 0; i < fromBitmap.dim.width; i++) {
                        if (count == 0) {
                            if (!getPackedNumber(count, rleMetrics)) {
                                return false;
                            }
                            black = !black;
                            // if (black) {
                            //   std::cout << count << ' ';
                            // }
                            // else {
                            //   std::cout << '(' << count << ')' << ' ';
                            // }
                        }
                        if (black) {
                            if (firstBlack < 0) {
                                firstBlack = i;
                            }
                            if constexpr (BLACK_ONE_BIT) {
                                if (inverted) {
                                    toRowPtr[idx] &= ~toMask;
                                } else {
                                    toRowPtr[idx] |= toMask;
                                }

                            } else {
                                if (inverted) {
                                    toRowPtr[idx] |= toMask;
                                } else {
                                    toRowPtr[idx] &= ~toMask;
                                }
                            }
                        }
                        toMask >>= 1;
                        if (toMask == 0) {
                            idx += 1;
                            toMask = 0x80;
                        }
                        count -= 1;
                    }

                    // if (repeatCount_ != 0) {
                    //     std::cout << "Repeat count: " << repeatCount_ << std::endl;
                    // }
                    while ((fromRow < fromBitmap.dim.height) && (repeatCount_-- > 0)) {
                        if (firstBlack >= 0) {
                            copyOneRowOneBit(toRowPtr, toRowPtr + toRowSize,
                                             atOffset.x + firstBlack,
                                             atOffset.x + fromBitmap.dim.width, inverted);
                        }
                        fromRow++;
                        toRowPtr += toRowSize;
                    }

                    repeatCount_ = 0;
                }
                // std::cout << std::endl;
            }
        } else {
            uint32_t toRowSize = toBitmap.dim.width;
            toRowPtr = toBitmap.pixels + static_cast<size_t>(atOffset.y * toRowSize);

            repeatCount_ = 0;
            nybbleFlipper_ = 0xf0U;

            if (rleMetrics.dynF == 14) { // is a bitmap?
                uint32_t count = 8;
                uint8_t data;

                for (uint32_t fromRow = 0; fromRow < (fromBitmap.dim.height);
                     fromRow++, toRowPtr += toRowSize) {
                    for (uint32_t toCol = atOffset.x; toCol < (fromBitmap.dim.width + atOffset.x);
                         toCol++) {
                        if (count >= 8) {
                            if (!getnext8(data)) {
                                std::cerr << "Not enough bitmap data!" << std::endl;
                                return false;
                            }
                            // std::cout << std::hex << +data << ' ';
                            count = 0;
                        }
                        if ((data & (0x80U >> count)) != 0) {
                            toRowPtr[toCol] = (inverted) ? WHITE_EIGHT_BITS : BLACK_EIGHT_BITS;
                        }
                        count++;
                    }
                }
                // std::cout << std::endl;
            } else {
                uint32_t count = 0;

                repeatCount_ = 0;
                nybbleFlipper_ = 0xf0U;

                bool black = !(rleMetrics.firstIsBlack == 1);

                for (uint32_t fromRow = 0; fromRow < (fromBitmap.dim.height);
                     fromRow++, toRowPtr += toRowSize) {
                    int firstBlack = -1;
                    for (uint32_t toCol = atOffset.x; toCol < (fromBitmap.dim.width + atOffset.x);
                         toCol++) {
                        if (count == 0) {
                            if (!getPackedNumber(count, rleMetrics)) {
                                return false;
                            }
                            black = !black;
                            // if (black) {
                            //   std::cout << count << ' ';
                            // }
                            // else {
                            //   std::cout << '(' << count << ')' << ' ';
                            // }
                        }
                        if (black) {
                            if (firstBlack < 0) {
                                firstBlack = toCol - atOffset.x;
                            }
                            toRowPtr[toCol] = (inverted) ? WHITE_EIGHT_BITS : BLACK_EIGHT_BITS;
                        }
                        count--;
                    }

                    // if (repeatCount_ != 0) std::cout << "Repeat count: " << repeatCount_ <<
                    // std::endl;
                    while ((fromRow < toBitmap.dim.height) && (repeatCount_-- > 0)) {
                        if (firstBlack >= 0) {
                            copyOneRowEightBits(toRowPtr, toRowPtr + toRowSize,
                                                atOffset.x + firstBlack, fromBitmap.dim.width);
                        }
                        fromRow++;
                        toRowPtr += toRowSize;
                    }

                    repeatCount_ = 0;
                }
                // std::cout << std::endl;
            }
        }
        return true;
    }
};