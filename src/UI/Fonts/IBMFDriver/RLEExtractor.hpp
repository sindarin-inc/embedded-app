#pragma once

#include <cinttypes>
#include <cstring>
#include <iostream>

#include "IBMFDefs.hpp"

using namespace IBMFDefs;

class RLEExtractor {
private:
    MemoryPtr memoryPtr, memoryEnd;

    uint32_t repeatCount;

    uint8_t nybbleFlipper = 0xf0U;
    uint8_t nybbleByte;

    const uint8_t PK_REPEAT_COUNT = 14;
    const uint8_t PK_REPEAT_ONCE = 15;

    bool getnext8(uint8_t &val) {
        if (memoryPtr >= memoryEnd) return false;
        val = *memoryPtr++;
        return true;
    }

    bool getNybble(uint8_t &nyb) {
        if (nybbleFlipper == 0xf0U) {
            if (!getnext8(nybbleByte)) return false;
            nyb = nybbleByte >> 4;
        } else {
            nyb = (nybbleByte & 0x0f);
        }
        nybbleFlipper ^= 0xffU;
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
    //     if repeatCount != 0 then abort('Extra repeat count!');
    //     if i = 14 then
    //        repeatCount := pkPackedNum
    //     else
    //        repeatCount := 1;
    //     send_out(true, repeatCount);
    //     pkPackedNum := pkPackedNum;
    //   end;
    // end;

    bool getPackedNumber(uint32_t &val, const RLEMetrics &rleMetrics) {
        uint8_t nyb;
        uint32_t i, j;

        uint8_t dynF = rleMetrics.dynF;

        while (true) {
            if (!getNybble(nyb)) return false;
            i = nyb;
            if (i == 0) {
                do {
                    if (!getNybble(nyb)) return false;
                    i++;
                } while (nyb == 0);
                j = nyb;
                while (i-- > 0) {
                    if (!getNybble(nyb)) return false;
                    j = (j << 4) + nyb;
                }
                val = j - 15 + ((13 - dynF) << 4) + dynF;
                break;
            } else if (i <= dynF) {
                val = i;
                break;
            } else if (i < PK_REPEAT_COUNT) {
                if (!getNybble(nyb)) return false;
                val = ((i - dynF - 1) << 4) + nyb + dynF + 1;
                break;
            } else {
                // if (repeatCount != 0) {
                //   std::cerr << "Spurious repeatCount iteration!" << std::endl;
                //   return false;
                // }
                if (i == PK_REPEAT_COUNT) {
                    if (!getPackedNumber(repeatCount, rleMetrics)) return false;
                } else { // i == PK_REPEAT_ONCE
                    repeatCount = 1;
                }
            }
        }
        return true;
    }

public:
    bool retrieveBitmap(const RLEBitmap &fromBitmap, Bitmap &toBitmap, const Pos atOffset,
                        const RLEMetrics rleMetrics) {
        // point on the glyphs' bitmap definition
        memoryPtr = fromBitmap.pixels;
        memoryEnd = memoryPtr + fromBitmap.length;
        uint8_t *rowp;

        if (resolution == PixelResolution::ONE_BIT) {
            uint32_t rowSize = (toBitmap.dim.width + 7) >> 3;
            rowp = toBitmap.pixels + (atOffset.y * rowSize);

            if (rleMetrics.dynF == 14) { // is a non-compressed RLE?
                uint32_t count = 8;
                uint8_t data;

                for (uint32_t row = 0; row < fromBitmap.dim.height; row++, rowp += rowSize) {
                    for (uint32_t col = atOffset.x; col < fromBitmap.dim.width + atOffset.x;
                         col++) {
                        if (count >= 8) {
                            if (!getnext8(data)) {
                                std::cerr << "Not enough bitmap data!" << std::endl;
                                return false;
                            }
                            // std::cout << std::hex << +data << ' ';
                            count = 0;
                        }
                        if (data & (0x80U >> count)) rowp[col >> 3] |= (0x80U >> (col & 7));
                        count++;
                    }
                }
                // std::cout << std::endl;
            } else {
                uint32_t count = 0;

                repeatCount = 0;
                nybbleFlipper = 0xf0U;

                bool black = !(rleMetrics.firstIsBlack == 1);

                for (uint32_t row = 0; row < fromBitmap.dim.height; row++, rowp += rowSize) {
                    for (uint32_t col = atOffset.x; col < fromBitmap.dim.width + atOffset.x;
                         col++) {
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
                        if (black) rowp[col >> 3] |= (0x80U >> (col & 0x07));
                        count--;
                    }

                    // if (repeatCount != 0) std::cout << "Repeat count: " << repeatCount <<
                    // std::endl;
                    while ((row < fromBitmap.dim.height) && (repeatCount-- > 0)) {
                        memcpy(rowp + rowSize, rowp, rowSize);
                        row++;
                        rowp += rowSize;
                    }

                    repeatCount = 0;
                }
                // std::cout << std::endl;
            }
        } else {
            uint32_t rowSize = toBitmap.dim.width;
            rowp = toBitmap.pixels + (atOffset.y * rowSize);

            repeatCount = 0;
            nybbleFlipper = 0xf0U;

            if (rleMetrics.dynF == 14) { // is a bitmap?
                uint32_t count = 8;
                uint8_t data;

                for (uint32_t row = 0; row < (fromBitmap.dim.height); row++, rowp += rowSize) {
                    for (uint32_t col = atOffset.x; col < (fromBitmap.dim.width + atOffset.x);
                         col++) {
                        if (count >= 8) {
                            if (!getnext8(data)) {
                                std::cerr << "Not enough bitmap data!" << std::endl;
                                return false;
                            }
                            // std::cout << std::hex << +data << ' ';
                            count = 0;
                        }
                        rowp[col] = (data & (0x80U >> count)) ? 0xFF : 0;
                        count++;
                    }
                }
                // std::cout << std::endl;
            } else {
                uint32_t count = 0;

                repeatCount = 0;
                nybbleFlipper = 0xf0U;

                bool black = !(rleMetrics.firstIsBlack == 1);

                for (uint32_t row = 0; row < (fromBitmap.dim.height); row++, rowp += rowSize) {
                    for (uint32_t col = atOffset.x; col < (fromBitmap.dim.width + atOffset.x);
                         col++) {
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
                        if (black) rowp[col] = 0xFF;
                        count--;
                    }

                    // if (repeatCount != 0) std::cout << "Repeat count: " << repeatCount <<
                    // std::endl;
                    while ((row < toBitmap.dim.height) && (repeatCount-- > 0)) {
                        memcpy(rowp + rowSize, rowp, rowSize);
                        row++;
                        rowp += rowSize;
                    }

                    repeatCount = 0;
                }
                // std::cout << std::endl;
            }
        }
        return true;
    }

public:
    RLEExtractor() {}
};