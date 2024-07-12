#pragma once

#include <driver/spi_master.h>

#include "config.h"

// Display resolution
#define EPD_WIDTH 400
#define EPD_HEIGHT 300

#define Seconds_1_5S 0
#define Seconds_1S 1

extern const unsigned char LUT_ALL[];

class Waveshare42 {
public:
    unsigned int width;
    unsigned int height;

    Waveshare42();
    ~Waveshare42();
    int Init(void);
    int Init_Fast(char mode);
    void Lut(void);
    int Init_4Gray(void);
    void SendCommand(unsigned char command);
    void SendData(unsigned char data);
    void ReadBusy(void);
    void Reset(void);
    void TurnOnDisplay(void);
    void TurnOnDisplay_Fast(void);
    void TurnOnDisplay_Partial(void);
    void TurnOnDisplay_4Gray(void);
    void Clear(void);
    void Display(const unsigned char *Image);
    void Display_Fast(const unsigned char *Image);
    void Display_Base(const unsigned char *Image);
    void Display_Partial(unsigned char *Image, unsigned int Xstart, unsigned int Ystart,
                         unsigned int Xend, unsigned int Yend);
    void Display_Partial_Not_refresh(unsigned char *Image, unsigned int Xstart, unsigned int Ystart,
                                     unsigned int Xend, unsigned int Yend);
    void Set_4GrayDisplay(const unsigned char *Image, int x, int y, int w, int l);
    void Sleep(void);

private:
    int IfInit();
    void DelayMs(unsigned int delaytime);
    void SpiTransfer(unsigned char data);
    unsigned int reset_pin;
    unsigned int dc_pin;
    unsigned int cs_pin;
    unsigned int busy_pin;

    bool inited = false;

    spi_device_handle_t spiHigh_{};
};