#ifndef _EPD_WAVEFORM_H_
#define _EPD_WAVEFORM_H_

#include <stdint.h>

#define MAX_WAVEFORM_VERSION 5

typedef struct
{
    uint32_t SourceData;
    uint16_t FrameCount;
    uint16_t Repeat;
} WaveformDataInit;

typedef struct
{
    int16_t FrameCount;
    uint8_t Reserved[2];
    uint8_t Data;
} WaveformDataBlock;

typedef struct
{
    uint16_t Version;
    uint16_t CheckSum;
    uint32_t DataSize;
    uint16_t TemperatureCount;
    uint16_t ModeCount;
    uint8_t Reserved[4];
} WaveformHeader202;

typedef struct
{
    uint16_t Version;
    uint16_t CheckSum;
    uint32_t DataSize;
    uint16_t TemperatureCount;
    uint16_t ModeCount;
    uint8_t Bpp;
    uint8_t srcDataMode;
    uint8_t Reserved[2];
} WaveformHeaderV3;

typedef struct
{
    uint16_t Version;
    uint16_t CheckSum;
    uint32_t DataSize;
    uint16_t TemperatureCount;
    uint16_t ModeCount;
    uint8_t Bpp;
    uint8_t srcDataMode;
    uint8_t Reserved[2];

    uint8_t Name[16];
    uint8_t StrVersion[16];
} WaveformHeaderV4;


typedef struct
{
    uint16_t Version;
    uint16_t CheckSum;
    uint32_t DataSize;
    uint16_t TemperatureCount;
    uint16_t ModeCount;
    uint8_t Bpp;
    uint8_t srcDataMode;
    uint8_t Reserved[2];

    uint8_t Name[16];
    uint8_t StrVersion[16];

    uint8_t StrEPD_CodeName[16];
    uint8_t FrameRate;
    int8_t TemperatureBiasStart;
    int8_t TemperatureBiasEnd;
    int8_t TemperatureBiasLUT[61];
} WaveformHeader;

typedef struct
{
    int16_t Temperautre;
    uint8_t Reserved[2];
    uint32_t DataAddress;
} TemperatureItem;

typedef struct
{
    const WaveformHeader *Header;
    const uint8_t *TemperatureTable;
    uint16_t TemperatureItemSize;
    uint8_t Available;
} WaveformInfo;

extern uint32_t EPD_WF_Init(const uint8_t *data);
extern const WaveformDataBlock *EPD_WF_GetData(int16_t temperature, uint8_t mode);
extern uint8_t EPD_WF_GetBpp(void);
extern uint16_t EPD_WF_Version(void);
extern uint8_t EPD_WF_SrcDataMode(void);

#endif
