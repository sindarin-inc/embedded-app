#include <EPD_TCON.h>
#include <EPD_Waveform.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void _Spi_SetClock(SPI_SPEED_TYPE speed);
static void _Spi_CS_Enable(void);
static void _Spi_CS_Disable(void);
static void _Spi_Write(const uint8_t *buf, int32_t len);
static uint8_t _Spi_ReadByte(void);
static void _SetResetPin(void);
static void _ClrResetPin(void);
static void _SetOEIPin(void);
static void _ClrOEIPin(void);
static uint32_t _ReadBusyPin(void);
static void _DelayMS(int32_t ms);
static void _OnFrameStartEvent(void);
static void _StartTimer(uint32_t ns);
static TIMER_STATE_TYPE _GetTimerState(void);

extern void EPD_DRV_SendScanLine(uint32_t gateCount, uint16_t *srcData);

extern const EPD_TCON_DRIVER_HAL g_TCON_HAL = {
    _Spi_SetClock, _Spi_CS_Enable,     _Spi_CS_Disable, _Spi_Write,     _Spi_ReadByte,
    _SetResetPin,  _ClrResetPin,       _SetOEIPin,      _ClrOEIPin,     _ReadBusyPin,
    _DelayMS,      _OnFrameStartEvent, _StartTimer,     _GetTimerState,
};

static void SPI_Init(void) { printf("SPI_Init()\n"); }

void EPD_HAL_Init(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY) {
    printf("EPD_HAL_Init()\n");
}

static void _Spi_CS_Enable(void) { printf("SPI"); }

static void _Spi_CS_Disable(void) { printf("\n"); }

static unsigned int BytesSent = 0;

static void _Spi_Write(const uint8_t *buf, int32_t len) {
    // printf("_Spi_Write([");
    BytesSent += len;
    printf(" (%d): ", len);
    for (int i = 0; i < len; i++) {
        printf("0x%02X", buf[i]);
        if (i != len - 1) {
            printf(", ");
        }
    }
    // printf("])\n");
}

static void _Spi_SetClock(SPI_SPEED_TYPE speed) { printf("_Spi_SetClock(%d)\n", speed); }

static uint8_t _Spi_ReadByte(void) {
    printf("_Spi_ReadByte\n");
    // MOSI is disconnected so should be fine to just return 0 here
    return 0x00;
}

static void _SetResetPin(void) { printf("_SetResetPin\n"); }

static void _ClrResetPin(void) { printf("_ClrResetPin\n"); }

static void _SetOEIPin(void) { printf("_SetOEIPin\n"); }

static void _ClrOEIPin(void) { printf("_ClrOEIPin\n"); }

static uint32_t _ReadBusyPin(void) {
    // printf("Read busy pin\n");
    return false;
}

static void _DelayMS(int32_t ms) { printf("Delay(%dms)\n", ms); }

static void _OnFrameStartEvent() { printf("Frame start\n"); }

static void _StartTimer(uint32_t ns) { printf("Timer started for %dns\n", ns); }

static TIMER_STATE_TYPE _GetTimerState(void) {
    printf("Timer requested, reporting STOP\n");
    return TIMER_STOP;
}

#define log_d(format, ...) printf(format "\n", ##__VA_ARGS__)

// These are defined by the lib/swtcon which we don't clang-tidy
// NOLINTNEXTLINE(readability-identifier-naming)
extern const EPD_TCON_DRIVER_HAL g_TCON_HAL;
// NOLINTNEXTLINE(readability-identifier-naming)
extern void EPD_HAL_Init(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY);
// NOLINTNEXTLINE(readability-identifier-naming)
extern const unsigned char g_WaveformData[];
// NOLINTNEXTLINE(readability-identifier-naming)
extern WaveformInfo _wf_info;
#define EPD_TCON_FRAME_RATE_HZ 24

int main(void) {
    printf("main\n");
    EPD_HAL_Init(0, 0, 0, 0);

    auto ret = EPD_TCON_Init(&g_TCON_HAL);
    if (ret < 0) {
        log_d("Bad return code from TCON Init: %d", ret);
        // esp_deep_sleep_start();
    }

    log_d("Loading waveform...");
    ret = EPD_TCON_LoadWaveform(g_WaveformData);
    log_d("Waveform loaded?");
    if (!ret) {
        log_d("Bad return code from waveform load: %d", ret);
        return 0;
    }

    log_d("_wf_info = WaveformInfo{");
    if (_wf_info.Header == nullptr) {
        log_d("  Header=nullptr,");
    } else {
        auto header = (WaveformHeaderV3 *)_wf_info.Header;
        log_d("  Header=WaveformHeaderV3{");
        log_d("    Version=%d,", header->Version);
        log_d("    CheckSum=%d,", header->CheckSum);
        log_d("    DataSize=%d,", header->DataSize);
        log_d("    TemperatureCount=%d,", header->TemperatureCount);
        log_d("    ModeCount=%d,", header->ModeCount);
        log_d("    Bpp=%d,", header->Bpp);
        log_d("    srcDataMode=%d,", header->srcDataMode);
        log_d("    Reserved[0]=%d,", header->Reserved[0]);
        log_d("    Reserved[1]=%d,", header->Reserved[1]);
        log_d("  };");
    }
    if (_wf_info.TemperatureTable == nullptr) {
        log_d("  TemperatureTable=nullptr,");
    } else {
        log_d("  TemperatureTable={");
        for (int i = 0; i < _wf_info.Header->TemperatureCount; ++i) {
            auto tempItem = (const TemperatureItem *)(_wf_info.TemperatureTable +
                                                      _wf_info.TemperatureItemSize * i);
            log_d("    Temperautre=%d", tempItem->Temperautre);
            log_d("    Reserved[0]=%d", tempItem->Reserved[0]);
            log_d("    Reserved[1]=%d", tempItem->Reserved[1]);
            // log_d("    DataAddress=%d", tempItem->DataAddress);
            for (int mode = 0; mode < _wf_info.Header->ModeCount; mode++) {
                auto offset = (&tempItem->DataAddress)[mode];
                auto waveform =
                    (const WaveformDataBlock *)((const uint8_t *)_wf_info.Header + offset);
                log_d("    DataAddress[mode=%d] (offset:%d) = WaveformDataBlock{", mode, offset);
                log_d("      FrameCount = %d", waveform->FrameCount);
                log_d("      Reserved[0] = %d", waveform->Reserved[0]);
                log_d("      Reserved[1] = %d", waveform->Reserved[1]);
                for (int i = 0; i < waveform->FrameCount; i++) {
                    auto frame = (uint8_t *)(&waveform->Data + 2 * i);
                    log_d("      Frame[%d] = 0x%02X, 0x%02X", i, frame[0], frame[1]);
                }
                log_d("    },");
            }
        }
        log_d("  }");
    }
    // Derived fields (not from the byte string)
    log_d("  TemperatureItemSize=%d,", _wf_info.TemperatureItemSize);
    log_d("  available=%d,", _wf_info.Available);
    log_d("};");

    // Written on back of each individual display - value is in mV so -2.81 on back should be -2810
    // here. -cojo
    log_d("SetVcom");
    EPD_TCON_SetVcom(-2810);
    log_d("SetFrameRate");
    EPD_TCON_SetFrameRate(EPD_TCON_FRAME_RATE_HZ);
    log_d("Powering on...");
    EPD_TCON_PowerOn();
    log_d("Powered on! Clearing panel...");
    EPD_TCON_PanelClear();

//     for (int gate = 0; gate < 512; gate++) {
//         uint8_t buffer[] = {
// 0x00, 0x00, 0x70, 0x0A, // Header (first two bytes are ignored by EPD_DRV_SendScanLine)
// 0x00, 0xFF, 0x00, 0x0C, // Border
// 0x00, 0x00, 0x00, 0x0C, // Gate Block 32 Low
// 0xC0, 0x00, // Gate Select Low
// // Start Data (32 bytes / 256 bits)
// // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// // 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// // End Data
// 0x00, 0x00, // Gate Select High
// 0x00, 0x00, 0x00, 0x00 // Gate Block 32 Low
//         };
//         EPD_DRV_SendScanLine(gate, (uint16_t *)buffer);
//     }

//     log_d("Panel cleared! Powering off...");
//     EPD_TCON_PowerOff();

//     // We might also consider trying some stuff with
//     // EPD_TCON_GetUpdateBuffer to write images and understand
//     // the output data.

//     log_d("Bytes sent: %d", BytesSent);

//     // uint8_t *fb = EPD_TCON_GetUpdateBuffer();

//     // for (i = 0; i < imgH; ++i) {
//     //     fb = EPD_TCON_GetUpdateBuffer();

//     //     fb += (y + i) * (EinkET013TT1_X_PIXELS / 4);
//     //     fb += (x >> 2);

//     //     // writeImage(fb, img + (i * (imgW / 8)), (imgW / 8));
//     //     for (i = 0; i < size; i`++) {
//     //         fb[i * 2] &= 0xf0;
//     //         fb[i * 2] |= img[i] >> 4;
//     //         fb[(i * 2) + 1] &= 0xf0;
//     //         fb[(i * 2) + 1] |= img[i] & 0x0f;
//     //     }
//     // }

    // writePartialImage(partialX_, partialY_, buffer_, partialW_, partialH_);
    // EPD_TCON_PowerOn();
    // EPD_TCON_Update(WF_MODE_GU4);
    EPD_TCON_PowerOff();
}
