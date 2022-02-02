#ifndef EPD_TCON_H
#define EPD_TCON_H

#include <stdint.h>

#define EPD_TCON_VERSION 202

#define WF_MODE_INIT 0
#define WF_MODE_GC4 1
#define WF_MODE_GU4 2
#define WF_MODE_A2 3
#define WF_MODE_GL 4

typedef enum
{
    SPI_SPEED_LOW = 0,
    SPI_SPEED_HIGH = 1
} SPI_SPEED_TYPE;

typedef enum
{
    TIMER_RUNNING = 1,
    TIMER_STOP = 0,
} TIMER_STATE_TYPE;

typedef enum
{
    EPD_BORDER_OFF = 0,
    EPD_BORDER_BLACK = 1,
    EPD_BORDER_WHITE = 2
} EPD_BORDER_TYPE;

typedef struct
{

    void (*Spi_SetClock)(SPI_SPEED_TYPE speed);
    void (*Spi_CS_Enable)(void);
    void (*Spi_CS_Disable)(void);
    void (*Spi_Write)(const uint8_t *buf, int32_t len);
    uint8_t (*Spi_ReadByte)(void);

    void (*SetResetPin)(void);
    void (*ClrResetPin)(void);
    void (*SetOEIPin)(void);
    void (*ClrOEIPin)(void);
    uint32_t (*ReadBusyPin)(void);

    void (*DelayMS)(int32_t ms);

    void (*OnFrameStartEvent)(void);

    void (*StartTimer)(uint32_t ns);
    TIMER_STATE_TYPE (*GetTimerState)
    ();
} EPD_TCON_DRIVER_HAL;

typedef struct
{
    int32_t Width;
    int32_t Height;
    int32_t BPP;
} EPD_TCON_PANEL_INFO;

extern const EPD_TCON_PANEL_INFO *EPD_TCON_GetPanelInfo(void);
extern void EPD_TCON_Update(uint8_t mode);
extern int32_t EPD_TCON_Init(const EPD_TCON_DRIVER_HAL *drvHal);
extern void EPD_TCON_PowerOff(void);
extern void EPD_TCON_PowerOn(void);
extern void EPD_TCON_PanelClear(void);
extern void EPD_TCON_SetVcom(int32_t mV);
extern uint8_t *EPD_TCON_GetUpdateBuffer(void);

extern uint32_t EPD_TCON_LoadWaveform(const uint8_t *data);

extern void EPD_TCON_Set_Border(EPD_BORDER_TYPE border);

extern uint16_t EPD_TCON_ReadTemperature(void);
extern void EPD_TCON_SetFrameRate(uint16_t fr);

#endif
