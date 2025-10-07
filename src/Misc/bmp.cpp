/**
 * @file bmp.c
 * @author Nilo Edson (niloedson.ms@gmail.com)
 * @brief Bitmap C library
 * @version 0.7
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022
 */

#include "bmp.h"

#include <Spiram/Memory.hpp>

auto BmpMalloc(size_t size) -> void * { return SpiramMalloc(size); }

auto BmpRead(RendererStream &stream) -> bmp_image * {
    bmp_image *img = nullptr;

    img = static_cast<bmp_image *>(BmpMalloc(sizeof(bmp_image)));
    if (img == nullptr) {
        return BmpCleanup(nullptr, img);
    }

    if (stream.readBytes(reinterpret_cast<uint8_t *>(&img->fileheader), sizeof(bmp_fileheader)) !=
        sizeof(bmp_fileheader)) {
        return BmpCleanup(nullptr, img);
    }

    if (stream.readBytes(reinterpret_cast<uint8_t *>(&img->dib.bmiHeader),
                         sizeof(bmp_infoheader)) != sizeof(bmp_infoheader)) {
        return BmpCleanup(nullptr, img);
    }

    if (img->dib.bmiHeader.biSize >= BMP_V4HEADER) {
        if (stream.readBytes(reinterpret_cast<uint8_t *>(&img->dib.bmiv4Header),
                             sizeof(bmp_v4header)) != sizeof(bmp_v4header)) {
            return BmpCleanup(nullptr, img);
        }
    }

    if (img->dib.bmiHeader.biSize >= BMP_V5HEADER) {
        if (stream.readBytes(reinterpret_cast<uint8_t *>(&img->dib.bmiv5Header),
                             sizeof(bmp_v5header)) != sizeof(bmp_v5header)) {
            return BmpCleanup(nullptr, img);
        }
    }

    if (BmpCheckheaders(img) == 0) {
        return BmpCleanup(nullptr, img);
    }

    uint32_t palettesize = BmpGetpalettesize(img);

    switch (img->dib.bmiHeader.biBitCount) {
    case BMP_1_BIT:
    case BMP_4_BITS:
    case BMP_8_BITS:
        img->dib.bmiColors = static_cast<bmp_rgbquad *>(BmpMalloc(palettesize));
        if (stream.readBytes(reinterpret_cast<uint8_t *>(img->dib.bmiColors), palettesize) !=
            palettesize) {
            return BmpCleanup(nullptr, img);
        }
        break;
    case BMP_16_BITS:
    case BMP_32_BITS:
        if (img->dib.bmiHeader.biCompression == BMP_BI_BITFIELDS) {
            img->dib.bmiColors = static_cast<bmp_rgbquad *>(BmpMalloc(palettesize));
            if (stream.readBytes(reinterpret_cast<uint8_t *>(img->dib.bmiColors), palettesize) !=
                palettesize) {
                return BmpCleanup(nullptr, img);
            }
        }
        break;
    case BMP_24_BITS:
        // never expect a BMP color palette
    default:
        break;
    }

    uint32_t datasize = BmpGetdatasize(img);

    img->ciPixelArray = static_cast<uint8_t *>(BmpMalloc(sizeof(uint8_t) * datasize));

    if (img->ciPixelArray == nullptr) {
        return BmpCleanup(nullptr, img);
    }

    if (stream.readBytes(img->ciPixelArray, sizeof(uint8_t) * datasize) !=
        sizeof(uint8_t) * datasize) {
        return BmpCleanup(nullptr, img);
    }

    return img;
}

auto BmpGetpixelcolor(bmp_image *img, int x, int y, bmp_color color) -> uint8_t {
    switch (img->dib.bmiHeader.biBitCount) {
    case BMP_0_BITS:
    case BMP_1_BIT:
    case BMP_2_BITS:
    case BMP_4_BITS:
        // TODO: add support for these formats.
        return 0;
        break;
    case BMP_8_BITS:
        return img->ciPixelArray[y * img->dib.bmiHeader.biWidth + x];
        break;
    case BMP_16_BITS:
        // TODO: add support for this format.
        return 0;
        break;
    case BMP_24_BITS:
        return img->ciPixelArray[3 * (y * img->dib.bmiHeader.biWidth + x) + color];
        break;
    case BMP_32_BITS:
        return img->ciPixelArray[4 * (y * img->dib.bmiHeader.biWidth + x) + color];
        break;

    default:
        return 0;
        break;
    }
}

auto BmpFindgray(uint8_t red, uint8_t green, uint8_t blue) -> uint8_t {
    double gray = 0.2989 * red + 0.5870 * green + 0.1140 * blue;
    return static_cast<uint8_t>(gray);
}

void BmpFiltercolor(bmp_image *img, bmp_color color) {
    // TODO: add support for 16 and 32 bits per pixel images.
    if (img->dib.bmiHeader.biBitCount != BMP_24_BITS) {
        return;
    }

    // TODO: add support for compressed images.
    if (img->dib.bmiHeader.biCompression != BMP_BI_RGB) {
        return;
    }

    uint32_t datasize = BmpGetdatasize(img);

    for (unsigned int pixel = color; pixel < datasize; pixel++) {
        if ((pixel % 3) != color) {
            img->ciPixelArray[pixel] = 0;
        }
    }
}

auto BmpCleanup(FILE *fptr, bmp_image *img) -> bmp_image * {
    if (fptr != nullptr) {
        if (fclose(fptr) != 0) {
            log_e("Failed to close file");
        }
    }

    if (img != nullptr) {
        if (img->dib.bmiColors != nullptr) {
            free(img->dib.bmiColors);
        }
        if (img->ciPixelArray != nullptr) {
            free(img->ciPixelArray);
        }
        free(img);
    }

    return nullptr;
}

auto BmpCheckheaders(bmp_image *img) -> int {
    if (img->fileheader.bfType != BMP_FILETYPE_BM) {
        return 0;
    }

    switch (img->dib.bmiHeader.biBitCount) {
    case BMP_0_BITS:
    case BMP_1_BIT:
    case BMP_2_BITS:
    case BMP_4_BITS:
    case BMP_8_BITS:
    case BMP_16_BITS:
    case BMP_24_BITS:
    case BMP_32_BITS:
        break;
    default:
        return 0;
    }

    if (img->dib.bmiHeader.biPlanes != BMP_DEFAULT_COLORPLANES) {
        return 0;
    }

    switch (img->dib.bmiHeader.biCompression) {
    case BMP_BI_RGB:
    case BMP_BI_RLE8:
    case BMP_BI_RLE4:
    case BMP_BI_BITFIELDS:
    case BMP_BI_JPEG:
    case BMP_BI_PNG:
    case BMP_BI_ALPHABITFIELDS:
    case BMP_BI_CMYK:
    case BMP_BI_CMYKRLE8:
    case BMP_BI_CMYKRLE4:
        break;
    default:
        return 0;
    }

    if (img->dib.bmiHeader.biSize >= BMP_V4HEADER) {
        switch (img->dib.bmiv4Header.bV4CSType) {
        case BMP_LCS_CALIBRATED_RGB:
        case BMP_LCS_sRGB:
        case BMP_LCS_WINDOWS_COLOR_SPACE:
        case BMP_LCS_PROFILE_LINKED:
        case BMP_LCS_PROFILE_EMBEDDED:
            break;
        default:
            return 0;
        }
    }

    return 1;
}

auto BmpGetfilesize(bmp_image *img) -> uint32_t { return img->fileheader.bfSize; }

auto BmpGetoffset(bmp_image *img) -> uint32_t { return img->fileheader.bfOffBits; }

auto BmpGetdatasize(bmp_image *img) -> uint32_t {
    return img->fileheader.bfSize - img->fileheader.bfOffBits;
}

auto BmpGetdibformat(bmp_image *img) -> uint32_t {
    switch (img->dib.bmiHeader.biSize) {
    case BMP_INFOHEADER:
        return BMP_INFOHEADER;
    case BMP_V4HEADER:
        return BMP_V4HEADER;
    case BMP_V5HEADER:
        return BMP_V5HEADER;
    default:
        return BMP_UNKNOWN_HEADER;
    }
}

auto BmpGetbitcount(bmp_image *img) -> uint16_t { return img->dib.bmiHeader.biBitCount; }

auto BmpGetpalettesize(bmp_image *img) -> uint32_t {
    switch (img->dib.bmiHeader.biBitCount) {
    case BMP_0_BITS:
    case BMP_1_BIT:
    case BMP_2_BITS:
    case BMP_4_BITS:
    case BMP_8_BITS:
        if (img->dib.bmiHeader.biClrUsed == 0) {
            return sizeof(bmp_rgbquad) * pow(2, img->dib.bmiHeader.biBitCount);
        } else {
            return sizeof(bmp_rgbquad) * img->dib.bmiHeader.biClrUsed;
        }
        break;
    case BMP_16_BITS:
        return sizeof(bmp_rgbquad) * 3;
        break;
    case BMP_32_BITS:
        return sizeof(bmp_rgbquad) * 4;
        break;
    case BMP_24_BITS:
        // never expect color palette
    default:
        return 0;
    }
}

auto BmpGetcompression(bmp_image *img) -> uint32_t {
    switch (img->dib.bmiHeader.biCompression) {
    case BMP_BI_RGB:
        return BMP_BI_RGB;
    case BMP_BI_RLE8:
        return BMP_BI_RLE8;
    case BMP_BI_RLE4:
        return BMP_BI_RLE4;
    case BMP_BI_BITFIELDS:
        return BMP_BI_BITFIELDS;
    case BMP_BI_JPEG:
        return BMP_BI_JPEG;
    case BMP_BI_PNG:
        return BMP_BI_PNG;
    case BMP_BI_ALPHABITFIELDS:
        return BMP_BI_ALPHABITFIELDS;
    case BMP_BI_CMYK:
        return BMP_BI_CMYK;
    case BMP_BI_CMYKRLE8:
        return BMP_BI_CMYKRLE8;
    case BMP_BI_CMYKRLE4:
        return BMP_BI_CMYKRLE4;
    default:
        return BMP_UNKNOWN_COMPRESSION;
    }
}

auto BmpGetnpixels(bmp_image *img) -> uint32_t {
    return img->dib.bmiHeader.biWidth * img->dib.bmiHeader.biHeight;
}

auto BmpGetncolors(bmp_image *img) -> uint32_t {
    switch (img->dib.bmiHeader.biBitCount) {
    case BMP_0_BITS:
    case BMP_1_BIT:
    case BMP_2_BITS:
    case BMP_4_BITS:
    case BMP_8_BITS:
        if (img->dib.bmiHeader.biClrUsed == 0) {
            return pow(2, img->dib.bmiHeader.biBitCount);
        } else {
            return img->dib.bmiHeader.biClrUsed;
        }
        break;
    case BMP_16_BITS:
        return 3;
        break;
    case BMP_32_BITS:
        return 4;
        break;
    case BMP_24_BITS:
        // never expect color palette
    default:
        return 0;
    }
}