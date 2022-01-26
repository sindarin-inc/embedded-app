#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "EPD_TCON.h"
#include "EPD_Driver.h"

#define Device_ID 0x00

#define OSC_250KHZ (0 << 5)
#define OSC_500KHZ (1 << 5)
#define OSC_1MHZ (2 << 5)
#define OSC_2MHZ (3 << 5)
#define OSC_SELECT OSC_2MHZ

#define GATE_LENGTH 512

static const EPD_TCON_DRIVER_HAL *g_drvHal;
static uint32_t _scanlineT = 67500;
static uint8_t _txBuffer[128] __attribute__((aligned(4)));

static const uint8_t _index_01_param[] = {0x1E, 0x00, 0xE0};

static const uint16_t _gateSelectTable_L[] =
    {
        0x00C0,
        0x0030,
        0x000C,
        0x0003,
        0xC000,
        0x3000,
        0x0C00,
        0x0300,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
};

static const uint16_t _gateSelectTable_H[] =
    {
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x00C0,
        0x0030,
        0x000C,
        0x0003,
        0xC000,
        0x3000,
        0x0C00,
        0x0300,
};
static const uint32_t _gateBlockTable32_L[] =
    {
        0x03000000,
        0x0C000000,
        0x30000000,
        0xC0000000,
        0x00030000,
        0x000C0000,
        0x00300000,
        0x00C00000,
        0x00000300,
        0x00000C00,
        0x00003000,
        0x0000C000,
        0x00000003,
        0x0000000C,
        0x00000030,
        0x000000C0,

        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
};

static const uint32_t _gateBlockTable32_H[] =
    {
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,

        0x000000C0,
        0x00000030,
        0x0000000C,
        0x00000003,
        0x0000C000,
        0x00003000,
        0x00000C00,
        0x00000300,
        0x00C00000,
        0x00300000,
        0x000C0000,
        0x00030000,
        0xC0000000,
        0x30000000,
        0x0C000000,
        0x03000000,
};

static const uint8_t _clearPacket[50] =
    {

        0x70, 0x0A, 0x00, 0x00,
        0x00, 0x03, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00

};

static const uint8_t _borderBlackPacket[50] =
    {

        0x70, 0x0A, 0x00, 0x03,
        0x00, 0x0C, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00};

static const uint8_t _borderWhitePacket[50] =
    {
        0x70, 0x0A, 0x00, 0x02,
        0x00, 0x0C, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00};

static unsigned char const Border_Black[4] = {
    0x00, 0x03, 0x00, 0x0C};

static unsigned char const Border_White[4] = {
    0x00, 0x02, 0x00, 0x0C};

static unsigned char const Border_None[4] = {
    0x00, 0x00, 0x00, 0x03};
static unsigned char const WithBorder[4] = {
    0x00, 0x00, 0x00, 0x0C};

void WithoutBorder(void)
{
    memcpy(&_txBuffer[4], &Border_None[0], 4);
}

void DisplayWhiteBorder(void)
{
    memcpy(&_txBuffer[4], &Border_White[0], 4);
}

void DisplayBlackBorder(void)
{
    memcpy(&_txBuffer[4], &Border_Black[0], 4);
}

static void WriteRegMultiData(uint8_t idx, const uint8_t *data, uint32_t len)
{
    uint8_t buf[2];

    buf[0] = 0x70;
    buf[1] = idx & 0x0F;

    g_drvHal->Spi_CS_Enable();
    g_drvHal->Spi_Write(buf, 2);
    g_drvHal->Spi_Write(data, len);
    g_drvHal->Spi_CS_Disable();
}

static void WriteRegData(uint8_t idx, const uint8_t data)
{
    uint8_t buf[3];
    buf[0] = 0x70;
    buf[1] = idx & 0x0F;
    buf[2] = data;
    g_drvHal->Spi_CS_Enable();
    g_drvHal->Spi_Write(buf, 3);
    g_drvHal->Spi_CS_Disable();
}

static void ReadRegData(int16_t idx, uint8_t *value)
{
    uint8_t result = 0;
    uint8_t buf[3];
    g_drvHal->Spi_SetClock(SPI_SPEED_LOW);

    buf[0] = 0x70;
    buf[1] = idx & 0x0F;
    g_drvHal->Spi_CS_Enable();
    g_drvHal->Spi_Write(buf, 2);
    g_drvHal->Spi_CS_Disable();

    buf[0] = 0x73;
    g_drvHal->Spi_CS_Enable();
    g_drvHal->Spi_Write(buf, 1);
    uint8_t prefix = g_drvHal->Spi_ReadByte();
    result = g_drvHal->Spi_ReadByte();
    g_drvHal->Spi_CS_Disable();

    *value = result;
    g_drvHal->Spi_SetClock(SPI_SPEED_HIGH);
}

static void SpiSendData(const uint8_t *data, int32_t len)
{
    g_drvHal->Spi_CS_Enable();
    g_drvHal->Spi_Write(data, len);
    g_drvHal->Spi_CS_Disable();
}

uint32_t EPD_DRV_CheckConnect()
{
    uint8_t data = 0x71;

    g_drvHal->Spi_SetClock(SPI_SPEED_LOW);

    g_drvHal->Spi_CS_Enable();
    g_drvHal->Spi_Write(&data, 1);
    data = g_drvHal->Spi_ReadByte();
    g_drvHal->Spi_CS_Disable();

    g_drvHal->Spi_SetClock(SPI_SPEED_HIGH);

    // TODO: Ignore this for now until MISO / SDO doesn't have to be jumpered (i.e. at integrated PICOD4 board rev) -cojo
    /*
    if ((data & 0x07) == Device_ID)
    {
        */
    return 1;
    /*
    }
    */

        return 0;
}

uint32_t EPD_DRV_Init(const EPD_TCON_DRIVER_HAL *drvHal)
{
    volatile uint32_t i;

    g_drvHal = drvHal;
    _scanlineT = 67500;

    g_drvHal->ClrResetPin();
    for (i = 0; i < 100000; ++i)
        ;
    g_drvHal->ClrOEIPin();

    for (i = 0; i < 100000; ++i)
        ;
    g_drvHal->SetResetPin();

    for (i = 0; i < 100000; ++i)
        ;

    ((uint32_t *)_txBuffer)[0] = 0x0A700000;

    for (i = 0; i < 10; ++i)
    {
        if (EPD_DRV_CheckConnect())
            break;
    }

    if (i >= 10)
        return 0;
    return 1;
}

void EPD_DRV_PowerOn(uint8_t vcom)
{
    g_drvHal->SetResetPin(); //

    WriteRegData(0x07, OSC_SELECT | 0x10);
    WriteRegData(0x08, 0x03);
    WriteRegMultiData(0x01, _index_01_param, sizeof(_index_01_param));

    WriteRegData(0x04, 0x0F);
    WriteRegData(0x09, vcom);
    WriteRegData(0x05, 0x00);
    g_drvHal->DelayMS(30);
    WriteRegData(0x05, 0x01);
    g_drvHal->DelayMS(30);
    WriteRegData(0x05, 0x03);
    g_drvHal->DelayMS(30);
    WriteRegData(0x05, 0x07);
    g_drvHal->DelayMS(60);
    WriteRegData(0x05, 0x0f);
    g_drvHal->DelayMS(30);
    WriteRegData(0x03, 0x10);
}

void EPD_DRV_PowerOff()
{
    WriteRegData(0x05, 0x07);
    g_drvHal->DelayMS(30);
    WriteRegData(0x05, 0x03);
    g_drvHal->DelayMS(30);
    WriteRegData(0x05, 0x01);
    g_drvHal->DelayMS(30);

    WriteRegData(0x0B, 0x00);

    WriteRegData(0x07, 0x40);

    WriteRegData(0x04, 0x80);
    WriteRegData(0x05, 0x00);

    g_drvHal->DelayMS(30);
    WriteRegData(0x08, 0x00);
    g_drvHal->DelayMS(30);

    WriteRegData(0x0B, 0x02);
}

void EPD_DRV_ResetOutput()
{
    SpiSendData(_clearPacket, 50);
    while (g_drvHal->ReadBusyPin())
        ;
    WriteRegData(0x03, 0x10);
}

int32_t EPD_DRV_SetBorderBlack(uint16_t frame, uint16_t delayms)
{
    volatile int32_t i = 0, f;

    for (f = 0; f < frame; f++)
    {
        SpiSendData(_borderWhitePacket, 50);

        while (g_drvHal->ReadBusyPin())
        {
            i++;

            //if (i > SCANLINE_TIMEOUT)
            //    return -1;
        }

        //WriteRegData(0x02, OE_PW_SETTING | 0x01);
        WriteRegData(0x03, 0x10);

        g_drvHal->DelayMS(delayms);
    }

    for (f = 0; f < frame; f++)
    {
        SpiSendData(_borderBlackPacket, 50);

        while (g_drvHal->ReadBusyPin())
        {
            i++;

            //if (i > SCANLINE_TIMEOUT)
            //    return -1;
        }

        //WriteRegData(0x02, OE_PW_SETTING | 0x01);
        WriteRegData(0x03, 0x10);

        g_drvHal->DelayMS(delayms);
    }

    //EPD_DRV_ResetOutput();

    return 0;
}

int32_t EPD_DRV_SetBorderWhite(uint16_t frame, uint16_t delayms)
{
    volatile int32_t i = 0, f;

    for (f = 0; f < frame; f++)
    {
        SpiSendData(_borderBlackPacket, 50);

        while (g_drvHal->ReadBusyPin())
        {
            i++;

            //if (i > SCANLINE_TIMEOUT)
            //    return -1;
        }

        //WriteRegData(0x02, OE_PW_SETTING | 0x01);
        WriteRegData(0x03, 0x10);

        g_drvHal->DelayMS(delayms);
    }

    for (f = 0; f < frame; f++)
    {
        SpiSendData(_borderWhitePacket, 50);

        while (g_drvHal->ReadBusyPin())
        {
            i++;

            //if (i > SCANLINE_TIMEOUT)
            //    return -1;
        }

        //WriteRegData(0x02, OE_PW_SETTING | 0x01);
        WriteRegData(0x03, 0x10);

        g_drvHal->DelayMS(delayms);
    }

    //EPD_DRV_ResetOutput();

    return 0;
}
void EPD_DRV_SetBorder(uint8_t borderCtl, uint8_t inputdata)
{
    if ((borderCtl == EPD_BORDER_BLACK) || (borderCtl == EPD_BORDER_WHITE))
    {
        _txBuffer[4] = WithBorder[0];
        _txBuffer[5] = inputdata;
        _txBuffer[6] = WithBorder[2];
        _txBuffer[7] = WithBorder[3];
    }
    else
    {
        _txBuffer[4] = Border_None[0];
        _txBuffer[5] = Border_None[1];
        _txBuffer[6] = Border_None[2];
        _txBuffer[7] = Border_None[3];
    }
}

void EPD_DRV_SendScanLine(uint32_t gateCount, uint16_t *srcData)
{
    uint32_t j; //, preGateCount, tableIndex;
    //uint32_t gateBlock, gateSelect;
    uint16_t *ptr;
    register uint16_t gs, gb;

    gs = gateCount & 0x0F;
    gb = gateCount >> 4;

    /*

    *((uint16_t *)&_txBuffer[42]) = 0;
    *((uint16_t *)&_txBuffer[8]) = 0;
    

    *((uint32_t *)&_txBuffer[44]) = 0;
    *((uint32_t *)&_txBuffer[4]) = 0;  
        

    SpiSendData(_txBuffer + 2, 48);  
    while (g_drvHal->ReadBusyPin());
    WriteRegData(0x03, 0x10);     
*/

    *((uint16_t *)&_txBuffer[12]) = _gateSelectTable_L[gs];
    *((uint16_t *)&_txBuffer[46]) = _gateSelectTable_H[gs];

    *((uint32_t *)&_txBuffer[8]) = _gateBlockTable32_L[gb];
    *((uint32_t *)&_txBuffer[48]) = _gateBlockTable32_H[gb];

    ptr = (uint16_t *)&_txBuffer[14];
    for (j = 0; j < 16; j++)
    {
        ptr[j] = *srcData;
        srcData++;
    }

    SpiSendData(_txBuffer + 2, 50);

#if 1
    while (g_drvHal->GetTimerState() == TIMER_RUNNING)
        ;
    g_drvHal->StartTimer(_scanlineT);
#endif

    while (g_drvHal->ReadBusyPin())
        ;
    WriteRegData(0x03, 0x10);
}

int16_t EPD_DRV_ReadPumpStatus()
{
    uint8_t value = 0xFF;

    ReadRegData(0x00, &value);

    return value;
}

int16_t EPD_DRV_ReadTemperature()
{
    uint8_t value = 0xFF;
    int16_t result = -1;

    ReadRegData(0x0F, &value);

    value &= 0x7F;

    if (value >= 0x76)
    {
        value |= 0x80;
        result = (int16_t)((int8_t)value * 2);
    }
    else if (value <= 0x22)
    {
        result = value * 2;
    }

    return result;
}

void EPD_DRV_SetFrameRate(uint16_t fr)
{
    if (fr < 1)
        fr = 1;

    if (fr > 100)
        fr = 100;
    _scanlineT = 1000000000 / fr;
    _scanlineT /= GATE_LENGTH;
    _scanlineT -= 380;
}

void EPD_DRV_OffScanLine()
{
    uint32_t j; //, preGateCount, tableIndex;
    //uint32_t gateBlock, gateSelect;
    uint16_t *ptr;
    //register uint16_t gs, gb;

    *((uint16_t *)&_txBuffer[12]) = 0;
    *((uint16_t *)&_txBuffer[46]) = 0;

    *((uint32_t *)&_txBuffer[8]) = 0;
    *((uint32_t *)&_txBuffer[48]) = 0;

    ptr = (uint16_t *)&_txBuffer[14];
    for (j = 0; j < 16; j++)
    {
        ptr[j] = 0;
    }

    WithoutBorder();

    SpiSendData(_txBuffer + 2, 50);
    WriteRegData(0x03, 0x10);
    while (g_drvHal->ReadBusyPin())
        ;
}
