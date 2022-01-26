#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "EPD_TCON.h"
#include "EPD_Driver.h"
#include "EPD_Waveform.h"

#define PANEL_BPP 1
#define PANEL_WIDTH 256
#define PANEL_HEIGHT 256

typedef struct
{
    uint16_t PanelWidth;
    uint16_t PanelHeight;
    uint16_t PanelBpp;
    uint16_t Vcom;
    uint16_t FrameRate;
    const WaveformDataBlock *WF_Data;
    const EPD_TCON_DRIVER_HAL *DriverHAL;
} EPD_Context;

static const EPD_TCON_PANEL_INFO _panelInfo =
    {
        PANEL_WIDTH,
        PANEL_HEIGHT,
        PANEL_BPP};

static EPD_Context _context;
static uint8_t _updateBuffer[(((PANEL_WIDTH * 2 * PANEL_BPP) + 7) / 8) * PANEL_HEIGHT] __attribute__((aligned(4)));
static uint8_t _scanLineBuffer[PANEL_WIDTH / 4] __attribute__((aligned(4)));

static uint8_t _lut[256];
static uint8_t g_border_Setting = 0;
static uint8_t g_borderAct = 0xFF;

typedef union
{
    uint8_t Value;
    struct
    {
        uint8_t C4 : 1;
        uint8_t C3 : 1;
        uint8_t C2 : 1;
        uint8_t C1 : 1;
        uint8_t P4 : 1;
        uint8_t P3 : 1;
        uint8_t P2 : 1;
        uint8_t P1 : 1;

    } Pixels;
} PixelData;

static void LUT_Preprocess(const uint8_t *wfDataPtr)
{
    PixelData pixel;
    uint8_t outValue;
    uint8_t bpp;
    uint16_t i;

    uint8_t srcValue[4][4];

    bpp = EPD_WF_GetBpp();

    switch (bpp)
    {
    case 1:
        srcValue[0][0] = wfDataPtr[0] & 0x03;
        srcValue[0][1] = (wfDataPtr[0] >> 2) & 0x03;
        srcValue[0][2] = wfDataPtr[1] & 0x03;
        srcValue[0][3] = (wfDataPtr[1] >> 2) & 0x03;
        break;
    case 2:
        srcValue[0][0] = wfDataPtr[0] & 0x03;
        srcValue[0][1] = (wfDataPtr[0] >> 6) & 0x03;
        srcValue[0][2] = wfDataPtr[3] & 0x03;
        srcValue[0][3] = (wfDataPtr[3] >> 6) & 0x03;
        break;
    case 4:
        srcValue[0][0] = wfDataPtr[0] & 0x03;
        srcValue[0][1] = (wfDataPtr[0] >> 6) & 0x03;
        srcValue[0][2] = wfDataPtr[12] & 0x03;
        srcValue[0][3] = (wfDataPtr[12] >> 6) & 0x03;
        break;
    default:
        srcValue[0][0] = 0;
        srcValue[0][1] = 0;
        srcValue[0][2] = 0;
        srcValue[0][3] = 0;
        break;
    }

    srcValue[1][0] = srcValue[0][0] << 2;
    srcValue[1][1] = srcValue[0][1] << 2;
    srcValue[1][2] = srcValue[0][2] << 2;
    srcValue[1][3] = srcValue[0][3] << 2;

    srcValue[2][0] = srcValue[0][0] << 4;
    srcValue[2][1] = srcValue[0][1] << 4;
    srcValue[2][2] = srcValue[0][2] << 4;
    srcValue[2][3] = srcValue[0][3] << 4;

    srcValue[3][0] = srcValue[0][0] << 6;
    srcValue[3][1] = srcValue[0][1] << 6;
    srcValue[3][2] = srcValue[0][2] << 6;
    srcValue[3][3] = srcValue[0][3] << 6;

    for (i = 0; i < 256; ++i)
    {
        pixel.Value = i;
        outValue = srcValue[3][(pixel.Pixels.P1 << 1) | pixel.Pixels.C1];
        outValue |= srcValue[2][(pixel.Pixels.P2 << 1) | pixel.Pixels.C2];
        outValue |= srcValue[1][(pixel.Pixels.P3 << 1) | pixel.Pixels.C3];
        outValue |= srcValue[0][(pixel.Pixels.P4 << 1) | pixel.Pixels.C4];
        _lut[i] = outValue;
    }
}

static void GetWaveformLUT(int16_t temp, uint8_t mode)
{
    const WaveformDataBlock *result;

    result = EPD_WF_GetData(temp, mode);
    if (result == _context.WF_Data)
    {
        return;
    }
    _context.WF_Data = result;
}

int32_t EPD_TCON_Init(const EPD_TCON_DRIVER_HAL *drvHal)
{
    if (drvHal == NULL)
        return -1;

    if (drvHal->ClrOEIPin == NULL ||
        drvHal->ClrResetPin == NULL ||
        drvHal->DelayMS == NULL ||
        drvHal->ReadBusyPin == NULL ||
        drvHal->SetOEIPin == NULL ||
        drvHal->SetResetPin == NULL ||
        drvHal->Spi_CS_Disable == NULL ||
        drvHal->Spi_CS_Enable == NULL ||
        drvHal->Spi_ReadByte == NULL ||
        drvHal->Spi_SetClock == NULL ||
        drvHal->Spi_Write == NULL ||
        drvHal->StartTimer == NULL ||
        drvHal->GetTimerState == NULL)
        return -2;

    _context.PanelHeight = PANEL_HEIGHT;
    _context.PanelWidth = PANEL_WIDTH;
    _context.PanelBpp = PANEL_BPP;
    _context.WF_Data = 0;
    _context.Vcom = 200;
    _context.DriverHAL = drvHal;
    _context.FrameRate = 50;

    if (!EPD_DRV_Init(drvHal))
        return -3;

    EPD_DRV_SetFrameRate(_context.FrameRate);
    g_border_Setting = EPD_BORDER_OFF;

    return 0;
}

void EPD_TCON_Set_Border(EPD_BORDER_TYPE border)
{
    switch (border)
    {
    case EPD_BORDER_OFF:
        g_border_Setting = EPD_BORDER_OFF;
        break;
    case EPD_BORDER_BLACK:
        g_border_Setting = EPD_BORDER_BLACK;
        g_borderAct = 0xF0;
        break;
    case EPD_BORDER_WHITE:
        g_border_Setting = EPD_BORDER_WHITE;
        g_borderAct = 0x0F;
        break;
    }
}

void EPD_TCON_Update(uint8_t mode)
{
    register uint8_t *ptr;
    register int32_t source;
    register uint8_t *buf;
    int32_t gate;
    const uint8_t *wfdataPtr;
    uint32_t frame;
    int16_t temperature, status;
    uint32_t wfdata_offset;

    uint16_t wfVersion;
    wfVersion = EPD_WF_Version();
    if ((mode == 0) && (wfVersion >= 4))
    {
        return;
    }

    switch (EPD_WF_GetBpp())
    {
    case 1:
        wfdata_offset = 2;
        break;
    case 2:
        wfdata_offset = 4;
        break;
    case 4:
        wfdata_offset = 64;
        break;
    default:
        wfdata_offset = 0;
        break;
    }

    status = EPD_DRV_ReadPumpStatus();
    temperature = EPD_DRV_ReadTemperature();

    GetWaveformLUT(temperature, mode);

    if (_context.WF_Data == NULL)
        return;

    wfdataPtr = &_context.WF_Data->Data;

    buf = _scanLineBuffer;

    for (frame = 0; frame < _context.WF_Data->FrameCount; frame++)
    {
        if (_context.DriverHAL->OnFrameStartEvent != NULL)
            _context.DriverHAL->OnFrameStartEvent();
        LUT_Preprocess(wfdataPtr);
        wfdataPtr += wfdata_offset; //next frame

        //Last line
        ptr = _updateBuffer + (256 / 4) * 255;

        for (gate = 0; gate < 256; gate++)
        {
            for (source = 0; source < (128 / 4); source++)
            {
                buf[source] = _lut[ptr[0]];
                ptr++;
            }

            //ptr += 128;
            ptr -= (256 + 128) / 4;
            EPD_DRV_SetBorder(g_border_Setting, _lut[g_borderAct]);
            EPD_DRV_SendScanLine(gate, (uint16_t *)_scanLineBuffer);
        }

        //ptr -= (256+128);
        //ptr -= ((256+128)/4);

        ptr = _updateBuffer + (256 / 4) * 255 + (128 / 4);
        for (gate = 256; gate < 512; gate++)
        {
            for (source = 0; source < (128 / 4); source++)
            {
                buf[source] = _lut[ptr[0]];
                ptr++;
            }

            //ptr -= (256+128);
            ptr -= (256 + 128) / 4;
            EPD_DRV_SetBorder(g_border_Setting, _lut[g_borderAct]);
            EPD_DRV_SendScanLine(gate, (uint16_t *)_scanLineBuffer);
        }

        while (_context.DriverHAL->GetTimerState() == TIMER_RUNNING)
            ; //for frame rate
        for (source = 0; source < 580; source++)
            ;
        EPD_DRV_ResetOutput();
    }

    for (gate = 0; gate < 256; gate++)
    {
        ptr = &_updateBuffer[gate * (256 / 4)];
        for (source = 0; source < (256 / 4); ++source)
        {
            *ptr = *ptr & 0x0F;
            *ptr |= *ptr << 4;
            ptr++;
        }
    }

    if (g_border_Setting != EPD_BORDER_OFF)
        g_borderAct = ((g_borderAct & 0x0F) << 4);
    g_border_Setting = EPD_BORDER_OFF;
}

uint8_t *EPD_TCON_GetUpdateBuffer()
{
    return _updateBuffer;
}

void EPD_TCON_PanelClear()
{
    uint32_t i;
    for (i = 0; i < sizeof(_updateBuffer); ++i)
    {
        _updateBuffer[i] |= 0x0F;
    }
    EPD_TCON_Set_Border(EPD_BORDER_WHITE);

    EPD_TCON_Update(0);
}

void EPD_TCON_PowerOn()
{
    EPD_DRV_PowerOn(_context.Vcom);
}

void EPD_TCON_PowerOff()
{
    EPD_DRV_PowerOff();
}

uint32_t EPD_TCON_LoadWaveform(const uint8_t *data)
{
    return EPD_WF_Init(data);
}

void EPD_TCON_SetVcom(int32_t mV)
{
    if (mV > 0)
        mV = 0;

    mV = -mV;

    if (mV >= 5600)
        mV = 5600;

    _context.Vcom = ((5600 - mV) * 255) / 5600;
}

const EPD_TCON_PANEL_INFO *EPD_TCON_GetPanelInfo()
{
    return &_panelInfo;
}

void EPD_TCON_UpdateTest(uint8_t value, int32_t frameCount)
{
    register int32_t source;
    register uint8_t *buf;
    int32_t gate;
    uint32_t frame;

    buf = _scanLineBuffer;

    for (frame = 0; frame < frameCount; frame++)
    {
        for (gate = 0; gate < 256; gate++)
        {
            //half
            for (source = 0; source < 32; source++)
            {
                buf[source] = value;
            }
            EPD_DRV_SendScanLine(gate, (uint16_t *)_scanLineBuffer);
        }

        for (gate = 511; gate >= 256; gate--)
        {

            for (source = 0; source < 32; source++)
            {
                buf[source] = value;
            }
            EPD_DRV_SendScanLine(gate, (uint16_t *)_scanLineBuffer);
        }

        for (source = 0; source < 580; source++)
            ;
        EPD_DRV_ResetOutput();
    }
}

void EPD_TCON_SetFrameRate(uint16_t fr)
{
    _context.FrameRate = fr;
    EPD_DRV_SetFrameRate(fr);
}

uint16_t EPD_TCON_GetFrameRate()
{
    return _context.FrameRate;
}

uint16_t EPD_TCON_ReadTemperature(void)
{
    return EPD_DRV_ReadTemperature();
}
