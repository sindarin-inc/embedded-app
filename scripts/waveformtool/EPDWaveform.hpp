#ifndef EPD_WAVEFORM_H
#define EPD_WAVEFORM_H

#include <cstdint>

#define MAX_WAVEFORM_VERSION 5

typedef struct {
    uint32_t sourceData;
    uint16_t frameCount;
    uint16_t repeat;
} WaveformDataInit;

typedef struct {
    int16_t frameCount;
    uint8_t reserved[2];
    uint8_t data;
} WaveformDataBlock;

typedef struct {
    uint16_t version;
    uint16_t checkSum;
    uint32_t dataSize;
    uint16_t temperatureCount;
    uint16_t modeCount;
    uint8_t reserved[4];
} WaveformHeader202;

typedef struct {
    uint16_t version;
    uint16_t checkSum;
    uint32_t dataSize;
    uint16_t temperatureCount;
    uint16_t modeCount;
    uint8_t bpp;
    uint8_t srcDataMode;
    uint8_t reserved[2];
} WaveformHeaderV3;

typedef struct {
    uint16_t version;
    uint16_t checkSum;
    uint32_t dataSize;
    uint16_t temperatureCount;
    uint16_t modeCount;
    uint8_t bpp;
    uint8_t srcDataMode;
    uint8_t reserved[2];

    uint8_t name[16];
    uint8_t strVersion[16];
} WaveformHeaderV4;

typedef struct {
    uint16_t version;
    uint16_t checkSum;
    uint32_t dataSize;
    uint16_t temperatureCount;
    uint16_t modeCount;
    uint8_t bpp;
    uint8_t srcDataMode;
    uint8_t reserved[2];

    uint8_t name[16];
    uint8_t strVersion[16];

    uint8_t strEpdCodeName[16];
    uint8_t frameRate;
    int8_t temperatureBiasStart;
    int8_t temperatureBiasEnd;
    int8_t temperatureBiasLut[61];
} WaveformHeader;

typedef struct {
    int16_t temperautre;
    uint8_t reserved[2];
    uint32_t dataAddress;
} TemperatureItem;

typedef struct {
    const WaveformHeader *header;
    const uint8_t *temperatureTable;
    uint16_t temperatureItemSize;
    uint8_t available;
} WaveformInfo;

extern WaveformInfo wfInfo;

auto EpdWfInit(const uint8_t *data) -> uint32_t;

#endif
