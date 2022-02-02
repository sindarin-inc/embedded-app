#include <EPD_TCON.h>
#include <EPD_Waveform.h>
#include <cstring>
#include <stdio.h>

#include "SPI.h"

// These are defined by the lib/swtcon which we don't clang-tidy
// NOLINTNEXTLINE(readability-identifier-naming)
extern const EPD_TCON_DRIVER_HAL g_TCON_HAL;
// NOLINTNEXTLINE(readability-identifier-naming)
extern void EPD_HAL_Init(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY);
// NOLINTNEXTLINE(readability-identifier-naming)
extern const unsigned char g_WaveformData[];
// NOLINTNEXTLINE(readability-identifier-naming)
extern WaveformInfo _wf_info;
extern void EPD_DRV_SendScanLine(uint32_t gateCount, uint16_t *srcData);

#define log_d(format, ...) printf(format "\n", ##__VA_ARGS__)

#define GET_BIT(value, index) (value & (1 << index)) >> index

int main() {
    EPD_HAL_Init(0, 1, 2, 3);

    auto ret = EPD_TCON_Init(&g_TCON_HAL);
    if (ret < 0) {
        printf("Bad return code from TCON Init: %d\n", ret);
        return 1;
    }

    ret = EPD_TCON_LoadWaveform(g_WaveformData);
    if (!ret) {
        printf("Bad return code from waveform load: %d\n", ret);
        return 1;
    }

    // log_d("_wf_info = WaveformInfo{");
    // if (_wf_info.Header == nullptr) {
    //     log_d("  Header=nullptr,");
    // } else {
    //     auto header = (WaveformHeaderV3 *)_wf_info.Header;
    //     log_d("  Header=WaveformHeaderV3{");
    //     log_d("    Version=%d,", header->Version);
    //     log_d("    CheckSum=%d,", header->CheckSum);
    //     log_d("    DataSize=%d,", header->DataSize);
    //     log_d("    TemperatureCount=%d,", header->TemperatureCount);
    //     log_d("    ModeCount=%d,", header->ModeCount);
    //     log_d("    Bpp=%d,", header->Bpp);
    //     log_d("    srcDataMode=%d,", header->srcDataMode);
    //     log_d("    Reserved[0]=%d,", header->Reserved[0]);
    //     log_d("    Reserved[1]=%d,", header->Reserved[1]);
    //     log_d("  };");
    // }
    // if (_wf_info.TemperatureTable == nullptr) {
    //     log_d("  TemperatureTable=nullptr,");
    // } else {
    //     log_d("  TemperatureTable={");
    //     for (int i = 0; i < _wf_info.Header->TemperatureCount; ++i) {
    //         auto tempItem = (const TemperatureItem *)(_wf_info.TemperatureTable +
    //                                                   _wf_info.TemperatureItemSize * i);
    //         log_d("    Temperautre=%d", tempItem->Temperautre);
    //         log_d("    Reserved[0]=%d", tempItem->Reserved[0]);
    //         log_d("    Reserved[1]=%d", tempItem->Reserved[1]);
    //         // log_d("    DataAddress=%d", tempItem->DataAddress);
    //         for (int mode = 0; mode < _wf_info.Header->ModeCount; mode++) {
    //             auto offset = (&tempItem->DataAddress)[mode];
    //             auto waveform =
    //                 (const WaveformDataBlock *)((const uint8_t *)_wf_info.Header + offset);
    //             log_d("    DataAddress[mode=%d] (offset:%d) = WaveformDataBlock{", mode, offset);
    //             log_d("      FrameCount = %d", waveform->FrameCount);
    //             log_d("      Reserved[0] = %d", waveform->Reserved[0]);
    //             log_d("      Reserved[1] = %d", waveform->Reserved[1]);
    //             for (int i = 0; i < waveform->FrameCount; i++) {
    //                 auto frame = (uint8_t *)(&waveform->Data + 2 * i);
    //                 log_d("      Frame[%d] = 0x%02X, 0x%02X", i, frame[0], frame[1]);
    //             }
    //             log_d("    },");
    //         }
    //     }
    //     log_d("  }");
    // }
    // // Derived fields (not from the byte string)
    // log_d("  TemperatureItemSize=%d,", _wf_info.TemperatureItemSize);
    // log_d("  available=%d,", _wf_info.Available);
    // log_d("};");

    EPD_TCON_SetVcom(-2810);
    EPD_TCON_SetFrameRate(24);
    EPD_TCON_PowerOn();
    EPD_TCON_PanelClear();
    EPD_TCON_PowerOff();

    printf("SETTING NEW FRAME WITH FULL REFRESH\n");

    // uint8_t buffer2[] = {
    //     // clang-format off
    //         // 0x00, 0x00, 0x70, 0x0A, // Header (first two bytes are ignored)
    //         // 0x00, 0x00, 0x00, 0x03, // Border
    //         // 0x00, 0x00, 0x00, 0x00, // Gate Block 32 Low (will be overwritten)
    //         // 0x00, 0x00, // Gate Select Low (will be overwritten)
    //         // Start Data (32 bytes / 256 bits)
    //         // 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    //         // 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    //         // 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    //         // 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,

    //         // 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
    //         // 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
    //         // 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
    //         // 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,

    //         // 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    //         // 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    //         // 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    //         // 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,

    //         0x12, 0x34, 0x56, 0x78, 0x22, 0x22, 0x22, 0x22,
    //         0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    //         0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    //         0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,

    //         // 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    //         // 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    //         // 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    //         // 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    //         // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    //         // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    //         // 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    //         // 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    //         // 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    //         // 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //         // 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //         // 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // // 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // // 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    // // 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    // // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

    // // 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    // // 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    // // 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,

    //             // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    //             // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    //             // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    //             // End Data
    //             // 0x00, 0x00, // Gate Select High
    //             // 0x00, 0x00, 0x00, 0x00 // Gate Block 32 Low
    //     // clang-format on
    // };

    // EPD_DRV_SendScanLine(0, (uint16_t *)buffer2);

    uint8_t *buffer = EPD_TCON_GetUpdateBuffer();

    for (int i = 0; i < 256 * 256; i += 7) {
        // The EPD driver indexes bits starting with the MSB first.
        // IE Bit 3 is the first bit read.
        // F7,FE,FF,FD
        // 1111 0111  1111 1110  1111 1111  1111 1101
        buffer[i / 4] &= ~(0x1 << (3 - i % 4));
    }

    // printf("BUFFER %02X,%02X,%02X,%02X,", buffer[0], buffer[1], buffer[2], buffer[3]);
    // Waveform frame 1: Frame[1] = 0x0E, 0x0C
    // 10 11 00 11 -- bb:white/bw:black/wb:none/ww:black
    // Buffer: F7,FE,FF,FD
    // 1111 0111  1111 1110  1111 1111  1111 1101
    // OLD:
    // F3 FF CF FF 3F FC FF,F3,FF,CF,FF,3F,FC,FF,F3,FF,CF,FF,3F,FC,FF,F3,FF,CF,FF,3F,FC,FF,F3,FF,CF
    // 1111 0011  1111 1111  1100 1111  1111 1111 0011 1111 1111 1100 1111 1111
    // sourceData[pixel / 4] &= ~(0x03 << (2 * (3 - pixel % 4)));

    // Gate 256
    // 3F,FC,FF,F3,FF,CF,FF,3F,FC,FF
    // 0011 1111 1111 1100 1111 1111 1111 0011 1111 1111 1100 1111 1111 1111 0011 1111 1111 1100
    // 1111 1111

    EPD_TCON_Set_Border(EPD_BORDER_WHITE);
    EPD_TCON_PowerOn();
    EPD_TCON_Update(WF_MODE_GC4);
    EPD_TCON_PowerOff();
}
