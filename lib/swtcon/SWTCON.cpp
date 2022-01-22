#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <Arduino.h>
#include <SPI.h>
#include <EPD_TCON.h>

#include <soc/cpu.h>
extern "C" {
    #include <esp32/clk.h>
}

static uint8_t csPin;
#define SPI_CS_Enable() digitalWrite(csPin, LOW)
#define SPI_CS_Disable() digitalWrite(csPin, HIGH)

static uint8_t oeiPin;
#define OEI_On() digitalWrite(oeiPin, HIGH)
#define OEI_Off() digitalWrite(oeiPin, LOW)

static uint8_t resetPin;
#define RESET_Active() digitalWrite(resetPin, LOW)
#define RESET_Release() digitalWrite(resetPin, HIGH)

static uint8_t busyPin;

#define MONITOR_FRAME_RATE_ENABLE 0
#define FR_MONITOR_PIN 0

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

static SPISettings SPI_Settings(8000000, SPI_MSBFIRST, SPI_MODE0);

extern const EPD_TCON_DRIVER_HAL g_TCON_HAL =
    {
        _Spi_SetClock,
        _Spi_CS_Enable,
        _Spi_CS_Disable,
        _Spi_Write,
        _Spi_ReadByte,

        _SetResetPin,
        _ClrResetPin,
        _SetOEIPin,
        _ClrOEIPin,
        _ReadBusyPin,
        _DelayMS,
        _OnFrameStartEvent,
        _StartTimer,
        _GetTimerState,
};

static void out_IO_Init(uint8_t GPIO_Pin)
{
    pinMode(GPIO_Pin, OUTPUT);
}

static void BUSY_IO_Init(uint8_t busy)
{
    busyPin = busy;
    pinMode(busyPin, INPUT);
}

static void OEI_IO_Init(uint8_t oei)
{
    oeiPin = oei;
    out_IO_Init(oeiPin);
}

static void POWERON_IO_Init(void)
{
    // out_IO_Init(POWERON_PIN);
}

static void RESET_IO_Init(uint8_t rst)
{
    resetPin = rst;
    out_IO_Init(resetPin);
}

static void SPI_CS_IO_Init(uint8_t cs)
{
    csPin = cs;
    out_IO_Init(csPin);
}

static void downSpiSpeedForRead(void)
{
    // No-op for now; both down and up are 8MHz at this time. -cojo
}

static void UpSpiSpeedForWrite(void)
{
    // No-op for now; both down and up are 8MHz at this time. -cojo
}

static void SPI_Init(void)
{
    SPI.begin();
}

void EPD_HAL_Init(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY)
{
    RESET_IO_Init(RST);
    POWERON_IO_Init();
    OEI_IO_Init(OEI);
    BUSY_IO_Init(BUSY);
    SPI_CS_IO_Init(CS);

    SPI_Init();

#if (MONITOR_FRAME_RATE_ENABLE)
    //out_IO_Init(RCC_AHB1Periph_GPIOE, GPIOE, GPIO_Pin_2);
    out_IO_Init(FR_MONITOR_RCC, FR_MONITOR_PORT, FR_MONITOR_PIN);
#endif
}

static void _Spi_CS_Enable(void)
{
    SPI.beginTransaction(SPI_Settings);
    SPI_CS_Enable();
}

static void _Spi_CS_Disable(void)
{
    SPI_CS_Disable();
    SPI.endTransaction();
}

static void _Spi_Write(const uint8_t *buf, int32_t len)
{
    SPI.writeBytes(buf, len);
}

static void _Spi_SetClock(SPI_SPEED_TYPE speed)
{
    if (speed == SPI_SPEED_LOW)
        downSpiSpeedForRead();
    else
        UpSpiSpeedForWrite();
}

static uint8_t _Spi_ReadByte(void)
{
    // Original STM example has two sends... is this correct? check if issues later... -cojo
    SPI.write(0xFF);
    return SPI.transfer(0xFF);
}

static void _SetResetPin(void)
{
    RESET_Release();
}

static void _ClrResetPin(void)
{
    RESET_Active();
}

static void _SetOEIPin(void)
{
    OEI_On();
}

static void _ClrOEIPin(void)
{
    OEI_Off();
}

static uint32_t _ReadBusyPin(void)
{
    int ret = digitalRead(busyPin);
    if (ret > 0)
    {
        Serial.printf("Busy: %d\n", ret);
    }
    return ret;
}

static void _DelayMS(int32_t ms)
{
    delay(ms);
}

static void _OnFrameStartEvent()
{
#if (MONITOR_FRAME_RATE_ENABLE)
    //GPIO_ToggleBits(GPIOE, GPIO_Pin_2);
    GPIO_ToggleBits(FR_MONITOR_PORT, FR_MONITOR_PIN);
#endif
}

static uint32_t timer_duration_cycles = 0;
static volatile uint32_t cycle_counts_start = 0;

static void _StartTimer(uint32_t ns) {
    RSR(CCOUNT, cycle_counts_start);
    timer_duration_cycles = (esp_clk_cpu_freq() / 1000000000.0) * ns;
}

static TIMER_STATE_TYPE _GetTimerState(void) {
    volatile uint32_t cycle_counts = 0;
    RSR(CCOUNT, cycle_counts);
    if (cycle_counts - cycle_counts_start < timer_duration_cycles) {
        return TIMER_RUNNING;
    }
    return TIMER_STOP;
}
