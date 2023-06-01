// Adapted from https://github.com/niloedson/Bitmap-C-Library (MIT license)
// Changed to support stream reading, and removed unused sections

/**
 * @file bmp.h
 * @author Nilo Edson (niloedson.ms@gmail.com)
 * @brief Bitmap C library
 * @version 0.7
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022
 */

#ifndef __BMP_H_
#define BMP_H_

#include "Renderers/RendererStream.hpp"

/* Includes -------------------------------------------------------------------*/
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defines --------------------------------------------------------------------*/
#define BMP_FILETYPE_BM 0x4d42
#define BMP_FILETYPE_BA 0x4142
#define BMP_FILETYPE_CI 0x4943
#define BMP_FILETYPE_CP 0x5043
#define BMP_FILETYPE_IC 0x4349
#define BMP_FILETYPE_PT 0x5054

#define BMP_DEFAULT_COLORPLANES 1

#define BMP_FILEHEADER_SIZE 14

// 16bpp bit masks ========================================
#define BMP_BITFIELDS_R5G5B5_R5 0x7C00
#define BMP_BITFIELDS_R5G5B5_G5 0x03E0
#define BMP_BITFIELDS_R5G5B5_B5 0x001F

#define BMP_BITFIELDS_R5G6B5_R5 0xF800
#define BMP_BITFIELDS_R5G6B5_G6 0x07E0
#define BMP_BITFIELDS_R5G6B5_B5 0x001F

#define BMP_ALPHABITFIELDS_R4G4B4A4_R4 0x0F00
#define BMP_ALPHABITFIELDS_R4G4B4A4_G4 0x00F0
#define BMP_ALPHABITFIELDS_R4G4B4A4_B4 0x000F
#define BMP_ALPHABITFIELDS_R4G4B4A4_A4 0xF000

// 32bpp bit masks ========================================
#define BMP_ALPHABITFIELDS_R8G8B8A8_R8 0x00FF0000
#define BMP_ALPHABITFIELDS_R8G8B8A8_G8 0x0000FF00
#define BMP_ALPHABITFIELDS_R8G8B8A8_B8 0x000000FF
#define BMP_ALPHABITFIELDS_R8G8B8A8_A8 0xFF000000

#define BMP_ALPHABITFIELDS_R8G7B9A5_R8 0x0001FE00
#define BMP_ALPHABITFIELDS_R8G7B9A5_G7 0x00FE0000
#define BMP_ALPHABITFIELDS_R8G7B9A5_B9 0x000001FF
#define BMP_ALPHABITFIELDS_R8G7B9A5_A5 0x1F000000

#pragma pack(1)

/* Enumerations ---------------------------------------------------------------*/

typedef enum bmp_dibformat {
    BMP_UNKNOWN_HEADER = 0,
    BMP_INFOHEADER = 40,
    BMP_V4HEADER = 108,
    BMP_V5HEADER = 124
} bmp_dibformat;

typedef enum bmp_bitcount {
    BMP_32_BITS = 32,
    BMP_24_BITS = 24,
    BMP_16_BITS = 16,
    BMP_8_BITS = 8,
    BMP_4_BITS = 4,
    BMP_2_BITS = 2,
    BMP_1_BIT = 1,
    BMP_0_BITS = 0
} bmp_bitcount;

typedef enum bmp_compression {
    BMP_UNKNOWN_COMPRESSION = 32,
    BMP_BI_RGB = 0,
    BMP_BI_RLE8 = 1,
    BMP_BI_RLE4 = 2,
    BMP_BI_BITFIELDS = 3,
    BMP_BI_JPEG = 4,
    BMP_BI_PNG = 5,
    BMP_BI_ALPHABITFIELDS = 6,
    BMP_BI_CMYK = 11,
    BMP_BI_CMYKRLE8 = 12,
    BMP_BI_CMYKRLE4 = 13
} bmp_compression;

typedef enum bmp_color {
    BMP_COLOR_ALPHA = 3,
    BMP_COLOR_RED = 2,
    BMP_COLOR_GREEN = 1,
    BMP_COLOR_BLUE = 0
} bmp_color;

typedef enum bmp_setncolours {
    BMP_SET_256_COLOURS = 256,
    BMP_SET_128_COLOURS = 128,
    BMP_SET_64_COLOURS = 64,
    BMP_SET_32_COLOURS = 32,
    BMP_SET_16_COLOURS = 16,
    BMP_SET_8_COLOURS = 8,
    BMP_SET_4_COLOURS = 4,
    BMP_SET_2_COLOURS = 2
} bmp_setncolours;

typedef enum bmp_padtype { BMP_PADTYPE_ZEROS, BMP_PADTYPE_REPLICATE } bmp_padtype;

// (from https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-logcolorspacea)
// (from
// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-wmf/eb4bbd50-b3ce-4917-895c-be31f214797f)
typedef enum bmp_bv4cstype {
    BMP_LCS_CALIBRATED_RGB = 0x00000000,
    BMP_LCS_sRGB = 0x73524742,
    BMP_LCS_WINDOWS_COLOR_SPACE = 0x57696E20
} bmp_bv4cstype;

// (from
// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-wmf/3c289fe1-c42e-42f6-b125-4b5fc49a2b20)
typedef enum bmp_bv5cstype {
    BMP_LCS_PROFILE_LINKED = 0x4C494E4B,
    BMP_LCS_PROFILE_EMBEDDED = 0x4D424544
} bmp_bv5cstype;

// (from https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv5header)
typedef enum bmp_bv5intent {
    BMP_LCS_GM_ABS_COLORIMETRIC,
    BMP_LCS_GM_BUSINESS,
    BMP_LCS_GM_GRAPHICS,
    BMP_LCS_GM_IMAGES
} bmp_bv5intent;

/* RLE structures -------------------------------------------------------------*/

#pragma pack(1)
typedef struct BmpRle8duo {
    uint8_t count;
    uint8_t value;
} bmp_rle8duo;

/* Coordinates structures -----------------------------------------------------*/

// CIEXYZ structure
#pragma pack(1)
typedef struct BmpCiexyz {
    uint32_t ciexyzX; // all 3 are FXPT2DOT30 type variables.
    uint32_t ciexyzY; // 32-bit floats
    uint32_t ciexyzZ; // still need to clarify these types
} bmp_ciexyz;

// CIEXYZTRIPLE structure
#pragma pack(1)
typedef struct BmpCiexyztriple {
    bmp_ciexyz ciexyzRed;
    bmp_ciexyz ciexyzGreen;
    bmp_ciexyz ciexyzBlue;
} bmp_ciexyztriple;

/* FILE Header ----------------------------------------------------------------*/

// BITMAPFILEHEADER structure
#pragma pack(1)
typedef struct BmpFileheader {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} bmp_fileheader;

/* DIB Headers ----------------------------------------------------------------*/

// BITMAPINFOHEADER structure
#pragma pack(1)
typedef struct BmpInfoheader {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} bmp_infoheader;

// BITMAPV4HEADER structure complementary piece
#pragma pack(1)
typedef struct BmpV4header {
    uint32_t bV4RedMask;
    uint32_t bV4GreenMask;
    uint32_t bV4BlueMask;
    uint32_t bV4AlphaMask;
    uint32_t bV4CSType;
    bmp_ciexyztriple bV4Endpoints;
    uint32_t bV4GammaRed;
    uint32_t bV4GammaGreen;
    uint32_t bV4GammaBlue;
} bmp_v4header;

// BITMAPV5HEADER structure complementary piece
#pragma pack(1)
typedef struct BmpV5header {
    uint32_t bV5Intent;
    uint32_t bV5ProfileData;
    uint32_t bV5ProfileSize;
    uint32_t bV5Reserved;
} bmp_v5header;

/* Colour Palettes ------------------------------------------------------------*/

// RGBQUAD structure
#pragma pack(1)
typedef struct BmpRgbquad {
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
    uint8_t rgbReserved;
} bmp_rgbquad;

// RGBTRIPLE structure
#pragma pack(1)
typedef struct BmpRgbtriple {
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
} bmp_rgbtriple;

/* Joint structures -----------------------------------------------------------*/

// BITMAPINFO structure bmp_dibheader
#pragma pack(1)
typedef struct BmpDibheader {
    bmp_infoheader bmiHeader;
    bmp_v4header bmiv4Header;
    bmp_v5header bmiv5Header;
    bmp_rgbquad *bmiColors;
} bmp_dibheader;

/* Main Structure -------------------------------------------------------------*/
//  (from https://docs.microsoft.com/en-us/windows/win32/gdi/bitmap-storage?redirectedfrom=MSDN)

#pragma pack(1)
typedef struct BmpImage {
    bmp_fileheader fileheader;
    bmp_dibheader dib;
    uint8_t *ciPixelArray;
} bmp_image;

/* Functions ------------------------------------------------------------------*/

/* file related functions -----------------------------------------------------*/

/**
 * @brief Read a Bitmap image and returns its metadata organized
 * in a <bmp_image> struct pointer.
 *
 * @param filename string specifying the filename to be read.
 * @return bmp_image* - pointer to the struct loaded with the
 *                      <filename> image data.
 */
auto BmpRead(RendererStream &stream) -> bmp_image *;

/* RGB functions --------------------------------------------------------------*/

/**
 * @brief Get the specified color value from the image[x,y] pixel.
 *
 * @param img pointer to the <bmp_image> metadata.
 * @param x the pixel x position.
 * @param y the pixel y position.
 * @param color the specified color.
 * @return uint8_t - color value retrieved.
 */
auto BmpGetpixelcolor(bmp_image *img, int x, int y, bmp_color color) -> uint8_t;

/**
 * @brief Find the gray-scale equivalent value from RGB entries.
 *
 * @param red red color entry value.
 * @param green green color entry value.
 * @param blue blue color entry value.
 * @return uint8_t - calculated gray-scale equivalent value.
 */
auto BmpFindgray(uint8_t red, uint8_t green, uint8_t blue) -> uint8_t;

/* helper functions -----------------------------------------------------------*/

/**
 * @brief Clean both pointers.
 *
 * @param fptr file pointer.
 * @param img <bmp_image> pointer.
 * @return bmp_image* always NULL.
 */
auto BmpCleanup(FILE *fptr, bmp_image *img) -> bmp_image *;

/**
 * @brief Check file headers to evaluate Bitmap conformity.
 *
 * @param img pointer to the <bmp_image> metadata.
 * @return int - returns 0 if headers have some problem, 1 otherwise.
 */
auto BmpCheckheaders(bmp_image *img) -> int;

/* utils ----------------------------------------------------------------------*/

/**
 * @brief Get file size from the <bmp_image> metadata.
 *
 * @param img <bmp_image> pointer.
 * @return uint32_t - the file size.
 */
auto BmpGetfilesize(bmp_image *img) -> uint32_t;

/**
 * @brief Get offset from the <bmp_image> metadata.
 *
 * @param img <bmp_image> pointer.
 * @return uint32_t - the offset.
 */
auto BmpGetoffset(bmp_image *img) -> uint32_t;

/**
 * @brief Get datasize from the <bmp_image> metadata.
 *
 * @param img <bmp_image> pointer.
 * @return uint32_t - the data size.
 */
auto BmpGetdatasize(bmp_image *img) -> uint32_t;

/**
 * @brief Get DIB header format from the <bmp_image> metadata.
 *
 * @param img <bmp_image> pointer.
 * @return uint32_t - the DIB header format.
 */
auto BmpGetdibformat(bmp_image *img) -> uint32_t;

/**
 * @brief Get bit count from the <bmp_image> metadata.
 *
 * @param img <bmp_image> pointer.
 * @return uint16_t - the bit count.
 */
auto BmpGetbitcount(bmp_image *img) -> uint16_t;

/**
 * @brief Get palette size from the <bmp_image> metadata.
 *
 * @param img <bmp_image> pointer.
 * @return uint32_t - the palette size.
 */
auto BmpGetpalettesize(bmp_image *img) -> uint32_t;

/**
 * @brief Get compression type from the <bmp_image> metadata.
 *
 * @param img <bmp_image> pointer.
 * @return uint32_t - the compression type.
 */
auto BmpGetcompression(bmp_image *img) -> uint32_t;

/**
 * @brief Get how many pixels are specified for this image.
 *
 * @param img <bmp_image> pointer.
 * @return uint32_t - the number of pixels.
 */
auto BmpGetnpixels(bmp_image *img) -> uint32_t;

/**
 * @brief Get how many colors are being used by this image.
 *
 * @param img <bmp_image> pointer.
 * @return uint32_t - the number of colors
 */
auto BmpGetncolors(bmp_image *img) -> uint32_t;

#endif