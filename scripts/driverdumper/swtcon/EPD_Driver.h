#ifndef EPD_DRIVER_H
#define EPD_DRIVER_H

#include <stdint.h>
#include "EPD_TCON.h"

#define SCAN_TYPE 0

extern int16_t EPD_DRV_ReadTemperature(void);
extern int16_t EPD_DRV_ReadPumpStatus(void);
extern void EPD_DRV_SendScanLine(uint32_t gateCount, uint16_t *srcData);
extern void EPD_DRV_ResetOutput(void);
extern void EPD_DRV_PowerOn(uint8_t vcom);
extern void EPD_DRV_PowerOff(void);
extern uint32_t EPD_DRV_Init(const EPD_TCON_DRIVER_HAL *drvHal);
extern int32_t EPD_DRV_SetBorderWhite(uint16_t frame, uint16_t delayms);
extern int32_t EPD_DRV_SetBorderBlack(uint16_t frame, uint16_t delayms);
extern void EPD_DRV_SetFrameRate(uint16_t fr);
extern void EPD_DRV_SetBorder(uint8_t borderCtl, uint8_t inputdata);

#endif
