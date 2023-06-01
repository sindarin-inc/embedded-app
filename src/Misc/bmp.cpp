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
#include "Misc/SpiramAllocator.hpp"

void* bmp_malloc(size_t size) {
    return SpiRamMalloc(size);
}

bmp_image * bmp_read(RendererStream& stream)
{
    bmp_image * img = NULL;
    
    img = (bmp_image*)bmp_malloc(sizeof(bmp_image));
    if (img == NULL) return bmp_cleanup(NULL, img);

    if (stream.readBytes((uint8_t*)&img->fileheader, sizeof(bmp_fileheader)) != sizeof(bmp_fileheader)) 
        return bmp_cleanup(NULL, img);

    if (stream.readBytes((uint8_t*)&img->dib.bmiHeader, sizeof(bmp_infoheader)) != sizeof(bmp_infoheader)) 
        return bmp_cleanup(NULL, img);

    if (img->dib.bmiHeader.biSize >= BMP_V4HEADER)
    {
        if (stream.readBytes((uint8_t*)&img->dib.bmiv4Header, sizeof(bmp_v4header)) != sizeof(bmp_v4header)) 
            return bmp_cleanup(NULL, img);
    }

    if (img->dib.bmiHeader.biSize >= BMP_V5HEADER)
    {
        if (stream.readBytes((uint8_t*)&img->dib.bmiv5Header, sizeof(bmp_v5header)) != sizeof(bmp_v5header)) 
            return bmp_cleanup(NULL, img);
    }

    if (bmp_checkheaders(img) == 0) return bmp_cleanup(NULL, img);

    uint32_t palettesize = bmp_getpalettesize(img);

    switch (img->dib.bmiHeader.biBitCount) {
    case BMP_1_BIT:
    case BMP_4_BITS:
    case BMP_8_BITS:
        img->dib.bmiColors = (bmp_rgbquad*)bmp_malloc(palettesize);
        if (stream.readBytes((uint8_t*)img->dib.bmiColors, palettesize) != palettesize) {
            return bmp_cleanup(NULL, img);
        }
        break;
    case BMP_16_BITS:
    case BMP_32_BITS:
        if (img->dib.bmiHeader.biCompression == BMP_BI_BITFIELDS) {
            img->dib.bmiColors = (bmp_rgbquad*)bmp_malloc(palettesize);
            if (stream.readBytes((uint8_t*)img->dib.bmiColors, palettesize) != palettesize) 
                return bmp_cleanup(NULL, img);
        }
        break;
    case BMP_24_BITS:
        // never expect a BMP color palette
    default:
        break;
    }
    
    uint32_t datasize = bmp_getdatasize(img);

    img->ciPixelArray = (uint8_t*)bmp_malloc(sizeof(uint8_t)*datasize);
    
    if (img->ciPixelArray == NULL) {
        return bmp_cleanup(NULL, img);
    }

    if (stream.readBytes(img->ciPixelArray, sizeof(uint8_t)*datasize) != sizeof(uint8_t)*datasize) {
        return bmp_cleanup(NULL, img);
    }

    return img;
}

uint8_t bmp_getpixelcolor(bmp_image * img, int x, int y, bmp_color color)
{
    switch (img->dib.bmiHeader.biBitCount)
    {
    case BMP_0_BITS:
    case BMP_1_BIT:
    case BMP_2_BITS:
    case BMP_4_BITS:
        //TODO: add support for these formats.
        return 0;
        break;
    case BMP_8_BITS:
        return img->ciPixelArray[y*img->dib.bmiHeader.biWidth+x];
        break;
    case BMP_16_BITS:
        //TODO: add support for this format.
        return 0;
        break;
    case BMP_24_BITS:
        return img->ciPixelArray[3*(y*img->dib.bmiHeader.biWidth+x)+color];
        break;
    case BMP_32_BITS:
        return img->ciPixelArray[4*(y*img->dib.bmiHeader.biWidth+x)+color];
        break;

    default:
        return 0;
        break;
    }
}

uint8_t bmp_findgray(uint8_t red, uint8_t green, uint8_t blue)
{
    double gray = 0.2989 * red + 0.5870 * green + 0.1140 * blue;
    return  (uint8_t) gray;
}

void bmp_filtercolor(bmp_image * img, bmp_color color)
{
    //TODO: add support for 16 and 32 bits per pixel images.
    if (img->dib.bmiHeader.biBitCount != BMP_24_BITS) return;
    
    //TODO: add support for compressed images.
    if (img->dib.bmiHeader.biCompression != BMP_BI_RGB) return;
    
    uint32_t datasize = bmp_getdatasize(img);

    for (unsigned int pixel = color; pixel < datasize; pixel++) {
        if ((pixel % 3) != color) {
            img->ciPixelArray[pixel] = 0;
        }
    }
}

void bmp_printdetails(bmp_image * img)
{
    printf("\n");
    printf("From BITMAPFILEHEADER: \n");

    printf("filetype:         \t%u  \t0x%x\n", img->fileheader.bfType, img->fileheader.bfType);
    printf("filesize:         \t%u  \t0x%x\n", img->fileheader.bfSize, img->fileheader.bfSize);
    printf("reserved1:        \t%u  \t0x%x\n", img->fileheader.bfReserved1, img->fileheader.bfReserved1);
    printf("reserved2:        \t%u  \t0x%x\n", img->fileheader.bfReserved2, img->fileheader.bfReserved2);
    printf("offset bits:      \t%u  \t0x%x\n", img->fileheader.bfOffBits, img->fileheader.bfOffBits);

    printf("\n");
    printf("From BITMAPINFOHEADER: \n");

    printf("size:             \t%u  \t0x%x\n", img->dib.bmiHeader.biSize, img->dib.bmiHeader.biSize);
    printf("width:            \t%u  \t0x%x\n", img->dib.bmiHeader.biWidth, img->dib.bmiHeader.biWidth);
    printf("height:           \t%u  \t0x%x\n", img->dib.bmiHeader.biHeight, img->dib.bmiHeader.biHeight);
    printf("planes:           \t%u  \t0x%x\n", img->dib.bmiHeader.biPlanes, img->dib.bmiHeader.biPlanes);
    printf("bits per pixel:   \t%u  \t0x%x\n", img->dib.bmiHeader.biBitCount, img->dib.bmiHeader.biBitCount);
    printf("compression:      \t%u  \t0x%x\n", img->dib.bmiHeader.biCompression, img->dib.bmiHeader.biCompression);
    printf("imagesize:        \t%u  \t0x%x\n", img->dib.bmiHeader.biSizeImage, img->dib.bmiHeader.biSizeImage);
    printf("xresolution:      \t%u  \t0x%x\n", img->dib.bmiHeader.biXPelsPerMeter, img->dib.bmiHeader.biXPelsPerMeter);
    printf("yresolution:      \t%u  \t0x%x\n", img->dib.bmiHeader.biYPelsPerMeter, img->dib.bmiHeader.biYPelsPerMeter);
    printf("colours used:     \t%u  \t0x%x\n", img->dib.bmiHeader.biClrUsed, img->dib.bmiHeader.biClrUsed);
    printf("main colours:     \t%u  \t0x%x\n", img->dib.bmiHeader.biClrImportant, img->dib.bmiHeader.biClrImportant);

    if (img->dib.bmiHeader.biSize >= BMP_V4HEADER)
    {
        printf("\n");
        printf("From BITMAPV4HEADER: \n");

        printf("red mask:       \t0x%x\n", img->dib.bmiv4Header.bV4RedMask);
        printf("green mask:     \t0x%x\n", img->dib.bmiv4Header.bV4GreenMask);
        printf("blue mask:      \t0x%x\n", img->dib.bmiv4Header.bV4BlueMask);
        printf("alpha mask:     \t0x%x\n", img->dib.bmiv4Header.bV4AlphaMask);
        printf("color space:    \t0x%x\n", img->dib.bmiv4Header.bV4CSType);
        printf("red (x):        \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzRed.ciexyzX);
        printf("red (y):        \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzRed.ciexyzY);
        printf("red (z):        \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzRed.ciexyzZ);
        printf("green (x):      \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzGreen.ciexyzX);
        printf("green (y):      \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzGreen.ciexyzY);
        printf("green (z):      \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzGreen.ciexyzZ);
        printf("blue (x):       \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzBlue.ciexyzX);
        printf("blue (y):       \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzBlue.ciexyzY);
        printf("blue (z):       \t0x%x\n", img->dib.bmiv4Header.bV4Endpoints.ciexyzBlue.ciexyzZ);
        printf("gamma red:      \t0x%x\n", img->dib.bmiv4Header.bV4GammaRed);
        printf("gamma green:    \t0x%x\n", img->dib.bmiv4Header.bV4GammaGreen);
        printf("gamma blue:     \t0x%x\n", img->dib.bmiv4Header.bV4GammaBlue);
    }
    
    if (img->dib.bmiHeader.biSize >= BMP_V5HEADER)
    {
        printf("\n");
        printf("From BITMAPV5HEADER: \n");

        printf("intent:         \t%u    \t0x%x\n", img->dib.bmiv5Header.bV5Intent, img->dib.bmiv5Header.bV5Intent);
        printf("profile data:   \t%u    \t0x%x\n", img->dib.bmiv5Header.bV5ProfileData, img->dib.bmiv5Header.bV5ProfileData);
        printf("profile size:   \t%u    \t0x%x\n", img->dib.bmiv5Header.bV5ProfileSize, img->dib.bmiv5Header.bV5ProfileSize);
        printf("reserved:       \t%u    \t0x%x\n", img->dib.bmiv5Header.bV5Reserved, img->dib.bmiv5Header.bV5Reserved);
    }

    printf("\n");
}

void bmp_printpixel(bmp_image * img, int x, int y)
{
    switch (img->dib.bmiHeader.biBitCount)
    {
    case BMP_1_BIT:
    case BMP_2_BITS:
    case BMP_4_BITS:
    case BMP_16_BITS:
        //TODO: implement support for these bpp.
        break;
    case BMP_8_BITS:
        printf(
            "img[%i][%i] = (%u)\n", x, y, 
            bmp_getpixelcolor(img, x, y, BMP_COLOR_RED)
        );
        break;
    case BMP_24_BITS:
        printf(
            "img[%i][%i] = (%u, %u, %u)\n", x, y, 
            bmp_getpixelcolor(img, x, y, BMP_COLOR_RED), 
            bmp_getpixelcolor(img, x, y, BMP_COLOR_GREEN), 
            bmp_getpixelcolor(img, x, y, BMP_COLOR_BLUE)
        );
        break;
    case BMP_32_BITS:
        printf(
            "img[%i][%i] = (%u, %u, %u, %u)\n", x, y, 
            bmp_getpixelcolor(img, x, y, BMP_COLOR_RED), 
            bmp_getpixelcolor(img, x, y, BMP_COLOR_GREEN), 
            bmp_getpixelcolor(img, x, y, BMP_COLOR_BLUE), 
            bmp_getpixelcolor(img, x, y, BMP_COLOR_ALPHA)
        );
        break;
    default:
        break;
    }
}

bmp_image * bmp_cleanup(FILE * fptr , bmp_image * img)
{
    if (fptr != NULL) fclose(fptr);

    if (img != NULL) {
        if (img->dib.bmiColors != NULL) free(img->dib.bmiColors);
        if (img->ciPixelArray != NULL) free(img->ciPixelArray);
        free(img);
    }

    return NULL;
}

int bmp_checkheaders(bmp_image * img)
{
    if (img->fileheader.bfType != BMP_FILETYPE_BM)
        return 0;

    switch (img->dib.bmiHeader.biBitCount) {
    case BMP_0_BITS: break;
    case BMP_1_BIT: break;
    case BMP_2_BITS: break;
    case BMP_4_BITS: break;
    case BMP_8_BITS: break;
    case BMP_16_BITS: break;
    case BMP_24_BITS: break;
    case BMP_32_BITS: break;
    default : return 0;
    }

    if (img->dib.bmiHeader.biPlanes != BMP_DEFAULT_COLORPLANES) 
        return 0;

    switch (img->dib.bmiHeader.biCompression) {
    case BMP_BI_RGB: break;
    case BMP_BI_RLE8: break;
    case BMP_BI_RLE4: break;
    case BMP_BI_BITFIELDS: break;
    case BMP_BI_JPEG: break;
    case BMP_BI_PNG: break;
    case BMP_BI_ALPHABITFIELDS: break;
    case BMP_BI_CMYK: break;
    case BMP_BI_CMYKRLE8: break;
    case BMP_BI_CMYKRLE4: break;
    default : return 0;
    }

    if (img->dib.bmiHeader.biSize >= BMP_V4HEADER)
    {
        switch (img->dib.bmiv4Header.bV4CSType)
        {
        case BMP_LCS_CALIBRATED_RGB: break;
        case BMP_LCS_sRGB: break;
        case BMP_LCS_WINDOWS_COLOR_SPACE: break;
        case BMP_LCS_PROFILE_LINKED: break;
        case BMP_LCS_PROFILE_EMBEDDED: break;
        default: return 0;
        }
    }
    
    return 1;    
}

uint32_t bmp_getfilesize(bmp_image * img)
{
    return img->fileheader.bfSize;
}

uint32_t bmp_getoffset(bmp_image * img)
{
    return img->fileheader.bfOffBits;
}

uint32_t bmp_getdatasize(bmp_image * img)
{
    return img->fileheader.bfSize - img->fileheader.bfOffBits;
}

uint32_t bmp_getdibformat(bmp_image * img)
{
    switch (img->dib.bmiHeader.biSize)
    {
    case BMP_INFOHEADER: return BMP_INFOHEADER;
    case BMP_V4HEADER: return BMP_V4HEADER;
    case BMP_V5HEADER: return BMP_V5HEADER;
    default:
        return BMP_UNKNOWN_HEADER;
    }
}

uint16_t bmp_getbitcount(bmp_image * img)
{
    return img->dib.bmiHeader.biBitCount;
}

uint32_t bmp_getpalettesize(bmp_image * img)
{
    switch (img->dib.bmiHeader.biBitCount)
    {
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
    default: return 0;
    }
}

uint32_t bmp_getcompression(bmp_image * img)
{
    switch (img->dib.bmiHeader.biCompression)
    {
    case BMP_BI_RGB: return BMP_BI_RGB;
    case BMP_BI_RLE8: return BMP_BI_RLE8;
    case BMP_BI_RLE4: return BMP_BI_RLE4;
    case BMP_BI_BITFIELDS: return BMP_BI_BITFIELDS;
    case BMP_BI_JPEG: return BMP_BI_JPEG;
    case BMP_BI_PNG: return BMP_BI_PNG;
    case BMP_BI_ALPHABITFIELDS: return BMP_BI_ALPHABITFIELDS;
    case BMP_BI_CMYK: return BMP_BI_CMYK;
    case BMP_BI_CMYKRLE8: return BMP_BI_CMYKRLE8;
    case BMP_BI_CMYKRLE4: return BMP_BI_CMYKRLE4;
    default:
        return BMP_UNKNOWN_COMPRESSION;
    }
}

uint32_t bmp_getnpixels(bmp_image * img)
{
    return img->dib.bmiHeader.biWidth * img->dib.bmiHeader.biHeight;
}

uint32_t bmp_getncolors(bmp_image * img)
{
    switch (img->dib.bmiHeader.biBitCount)
    {
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
    default: return 0;
    }
}