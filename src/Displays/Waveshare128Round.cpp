#if CONFIG_DISPLAY_WAVESHARE_128ROUND

#include "Waveshare128Round.hpp"

#include <algorithm>
#include <cstring>
#include <esp_err.h>
#include <math.h>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

// esp_clk methods are deprecated in IDF 5+
// TODO: replace
extern "C" {
#include <esp_private/esp_clk.h>
}

#define LCD_DC_PIN DISPLAY_WAVESHARE_128ROUND_DC
#define LCD_RST_PIN DISPLAY_WAVESHARE_128ROUND_RST
#define LCD_CS_PIN DISPLAY_WAVESHARE_128ROUND_CS

#define HORIZONTAL 0
#define VERTICAL 1

uint8_t inline reverse8Bits(uint8_t value) {
    uint8_t reversed = 0;
    for (int i = 0; i < 8; ++i) {
        if (value & (1 << i)) {
            reversed |= (1 << (7 - i));
        }
    }
    return reversed;
}

/**
 * @brief Construct a new Waveshare128Round object
 *
 * @param clk The clock pin
 * @param mosi The MOSI pin
 * @param cs The display chip select pin - **NOTE** this is ACTIVE HIGH!
 */
Waveshare128Round::Waveshare128Round(uint8_t clk, uint8_t mosi, uint8_t cs) {
    cs_ = cs;
    mosi_ = mosi;
    clk_ = clk;
}

/**
 * @brief Start the driver object, setting up pins and configuring a buffer for
 * the screen contents
 *
 * @return boolean true: success false: failure
 */
bool Waveshare128Round::begin(void) {
    PinMode(cs_, OUTPUT);

    // this display is weird in that cs_ is active HIGH not LOW like every other
    // SPI device
    DigitalWrite(cs_, LOW);

    spi_bus_config_t buscfg = {
        .mosi_io_num = DISPLAY_WAVESHARE_128ROUND_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = DISPLAY_WAVESHARE_128ROUND_CLK,
        // These must be unset or the SPI driver will try to use them
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPI_DEVICE_TXBIT_LSBFIRST,
    };

    esp_err_t err = spi_bus_initialize(DISPLAY_SPI, &buscfg, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        log_e("Error initializing SPI bus %d", err);
        return false;
    }

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .mode = 0,

        .clock_source = SPI_CLK_SRC_DEFAULT,
        .clock_speed_hz = 2 * 1000 * 1000,

        .spics_io_num = -1,
        .queue_size = 1,
    };

    err = spi_bus_add_device(DISPLAY_SPI, &devcfg, &spiHigh_);
    if (err != ESP_OK) {
        log_e("Error adding display device to SPI bus: %d", err);
        return false;
    }


    Init(HORIZONTAL);
    Clear(0XFFFF);
    
    return true;
}

void inline Waveshare128Round::writeSPI(const uint8_t *data, size_t len) {
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    esp_err_t err = spi_device_polling_transmit(spiHigh_, &t);

    if (err != ESP_OK) {
        log_e("Error writing to SPI of length %d", len);
    }
}


LCD_1IN28_ATTRIBUTES LCD_1IN28;


/******************************************************************************
function :	Hardware reset
parameter:
******************************************************************************/
void  Waveshare128Round::Reset(void)
{
    DigitalWrite(LCD_RST_PIN, 1);
    Delay(100);
    DigitalWrite(LCD_RST_PIN, 0);
    Delay(100);
    DigitalWrite(LCD_RST_PIN, 1);
	DigitalWrite(LCD_CS_PIN, 0);
    Delay(100);
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/
void Waveshare128Round::SendCommand(UBYTE Reg)
{
    DigitalWrite(LCD_DC_PIN, 0);
    //DigitalWrite(LCD_CS_PIN, 0);
    writeSPI(&Reg, 1);
    //DigitalWrite(LCD_CS_PIN, 1);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
void Waveshare128Round::SendData_8Bit(UBYTE Data)
{
    DigitalWrite(LCD_DC_PIN, 1);
    //DigitalWrite(LCD_CS_PIN, 0);
    writeSPI(&Data, 1);
    //DigitalWrite(LCD_CS_PIN, 1);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
void Waveshare128Round::SendData_16Bit(UWORD Data)
{
    DigitalWrite(LCD_DC_PIN, 1);
    //DigitalWrite(LCD_CS_PIN, 0);
	UBYTE low = Data & 0xFF;
	UBYTE high = (Data >> 8) & 0xFF;
    writeSPI(&high, 1);
    writeSPI(&low, 1);
   // DigitalWrite(LCD_CS_PIN, 1);
	
}

/******************************************************************************
function :	Initialize the lcd register
parameter:
******************************************************************************/
void Waveshare128Round::InitReg(void)
{
    SendCommand(0xEF);
	SendCommand(0xEB);
	SendData_8Bit(0x14); 
	
    SendCommand(0xFE);			 
	SendCommand(0xEF); 

	SendCommand(0xEB);	
	SendData_8Bit(0x14); 

	SendCommand(0x84);			
	SendData_8Bit(0x40); 

	SendCommand(0x85);			
	SendData_8Bit(0xFF); 

	SendCommand(0x86);			
	SendData_8Bit(0xFF); 

	SendCommand(0x87);			
	SendData_8Bit(0xFF);

	SendCommand(0x88);			
	SendData_8Bit(0x0A);

	SendCommand(0x89);			
	SendData_8Bit(0x21); 

	SendCommand(0x8A);			
	SendData_8Bit(0x00); 

	SendCommand(0x8B);			
	SendData_8Bit(0x80); 

	SendCommand(0x8C);			
	SendData_8Bit(0x01); 

	SendCommand(0x8D);			
	SendData_8Bit(0x01); 

	SendCommand(0x8E);			
	SendData_8Bit(0xFF); 

	SendCommand(0x8F);			
	SendData_8Bit(0xFF); 


	SendCommand(0xB6);
	SendData_8Bit(0x00);
	SendData_8Bit(0x20);

	SendCommand(0x36);
	SendData_8Bit(0x08);//Set as vertical screen

	SendCommand(0x3A);			
	SendData_8Bit(0x05); 


	SendCommand(0x90);			
	SendData_8Bit(0x08);
	SendData_8Bit(0x08);
	SendData_8Bit(0x08);
	SendData_8Bit(0x08); 

	SendCommand(0xBD);			
	SendData_8Bit(0x06);
	
	SendCommand(0xBC);			
	SendData_8Bit(0x00);	

	SendCommand(0xFF);			
	SendData_8Bit(0x60);
	SendData_8Bit(0x01);
	SendData_8Bit(0x04);

	SendCommand(0xC3);			
	SendData_8Bit(0x13);
	SendCommand(0xC4);			
	SendData_8Bit(0x13);

	SendCommand(0xC9);			
	SendData_8Bit(0x22);

	SendCommand(0xBE);			
	SendData_8Bit(0x11); 

	SendCommand(0xE1);			
	SendData_8Bit(0x10);
	SendData_8Bit(0x0E);

	SendCommand(0xDF);			
	SendData_8Bit(0x21);
	SendData_8Bit(0x0c);
	SendData_8Bit(0x02);

	SendCommand(0xF0);   
	SendData_8Bit(0x45);
	SendData_8Bit(0x09);
	SendData_8Bit(0x08);
	SendData_8Bit(0x08);
	SendData_8Bit(0x26);
 	SendData_8Bit(0x2A);

 	SendCommand(0xF1);    
 	SendData_8Bit(0x43);
 	SendData_8Bit(0x70);
 	SendData_8Bit(0x72);
 	SendData_8Bit(0x36);
 	SendData_8Bit(0x37);  
 	SendData_8Bit(0x6F);


 	SendCommand(0xF2);   
 	SendData_8Bit(0x45);
 	SendData_8Bit(0x09);
 	SendData_8Bit(0x08);
 	SendData_8Bit(0x08);
 	SendData_8Bit(0x26);
 	SendData_8Bit(0x2A);

 	SendCommand(0xF3);   
 	SendData_8Bit(0x43);
 	SendData_8Bit(0x70);
 	SendData_8Bit(0x72);
 	SendData_8Bit(0x36);
 	SendData_8Bit(0x37); 
 	SendData_8Bit(0x6F);

	SendCommand(0xED);	
	SendData_8Bit(0x1B); 
	SendData_8Bit(0x0B); 

	SendCommand(0xAE);			
	SendData_8Bit(0x77);
	
	SendCommand(0xCD);			
	SendData_8Bit(0x63);		


	SendCommand(0x70);			
	SendData_8Bit(0x07);
	SendData_8Bit(0x07);
	SendData_8Bit(0x04);
	SendData_8Bit(0x0E); 
	SendData_8Bit(0x0F); 
	SendData_8Bit(0x09);
	SendData_8Bit(0x07);
	SendData_8Bit(0x08);
	SendData_8Bit(0x03);

	SendCommand(0xE8);			
	SendData_8Bit(0x34);

	SendCommand(0x62);			
	SendData_8Bit(0x18);
	SendData_8Bit(0x0D);
	SendData_8Bit(0x71);
	SendData_8Bit(0xED);
	SendData_8Bit(0x70); 
	SendData_8Bit(0x70);
	SendData_8Bit(0x18);
	SendData_8Bit(0x0F);
	SendData_8Bit(0x71);
	SendData_8Bit(0xEF);
	SendData_8Bit(0x70); 
	SendData_8Bit(0x70);

	SendCommand(0x63);			
	SendData_8Bit(0x18);
	SendData_8Bit(0x11);
	SendData_8Bit(0x71);
	SendData_8Bit(0xF1);
	SendData_8Bit(0x70); 
	SendData_8Bit(0x70);
	SendData_8Bit(0x18);
	SendData_8Bit(0x13);
	SendData_8Bit(0x71);
	SendData_8Bit(0xF3);
	SendData_8Bit(0x70); 
	SendData_8Bit(0x70);

	SendCommand(0x64);			
	SendData_8Bit(0x28);
	SendData_8Bit(0x29);
	SendData_8Bit(0xF1);
	SendData_8Bit(0x01);
	SendData_8Bit(0xF1);
	SendData_8Bit(0x00);
	SendData_8Bit(0x07);

	SendCommand(0x66);			
	SendData_8Bit(0x3C);
	SendData_8Bit(0x00);
	SendData_8Bit(0xCD);
	SendData_8Bit(0x67);
	SendData_8Bit(0x45);
	SendData_8Bit(0x45);
	SendData_8Bit(0x10);
	SendData_8Bit(0x00);
	SendData_8Bit(0x00);
	SendData_8Bit(0x00);

	SendCommand(0x67);			
	SendData_8Bit(0x00);
	SendData_8Bit(0x3C);
	SendData_8Bit(0x00);
	SendData_8Bit(0x00);
	SendData_8Bit(0x00);
	SendData_8Bit(0x01);
	SendData_8Bit(0x54);
	SendData_8Bit(0x10);
	SendData_8Bit(0x32);
	SendData_8Bit(0x98);

	SendCommand(0x74);			
	SendData_8Bit(0x10);	
	SendData_8Bit(0x85);	
	SendData_8Bit(0x80);
	SendData_8Bit(0x00); 
	SendData_8Bit(0x00); 
	SendData_8Bit(0x4E);
	SendData_8Bit(0x00);					
	
    SendCommand(0x98);			
	SendData_8Bit(0x3e);
	SendData_8Bit(0x07);

	SendCommand(0x35);	
	SendCommand(0x21);

	SendCommand(0x11);
	Delay(120);
	SendCommand(0x29);
	Delay(20);
}

/********************************************************************************
function:	Set the resolution and scanning method of the screen
parameter:
		Scan_dir:   Scan direction
********************************************************************************/
void Waveshare128Round::SetAttributes(UBYTE Scan_dir)
{
    //Get the screen scan direction
    LCD_1IN28.SCAN_DIR = Scan_dir;
    UBYTE MemoryAccessReg = 0x08;

    //Get GRAM and LCD width and height
    if(Scan_dir == HORIZONTAL) {
        LCD_1IN28.HEIGHT	= LCD_1IN28_HEIGHT;
        LCD_1IN28.WIDTH   = LCD_1IN28_WIDTH;
        MemoryAccessReg = 0Xc8;
    } else {
        LCD_1IN28.HEIGHT	= LCD_1IN28_WIDTH;
        LCD_1IN28.WIDTH   = LCD_1IN28_HEIGHT;
        MemoryAccessReg = 0X68;
    }

    // Set the read / write scan direction of the frame memory
    SendCommand(0x36); //MX, MY, RGB mode
    //SendData_8Bit(MemoryAccessReg);	//0x08 set RGB
	SendData_8Bit(MemoryAccessReg);	//0x08 set RGB
}

/********************************************************************************
function :	Initialize the lcd
parameter:
********************************************************************************/
void Waveshare128Round::Init(UBYTE Scan_dir)
{
    //Turn on the backlight
    //DEV_SET_PWM(100);
    //Hardware reset
    Reset();

    //Set the resolution and scanning method of the screen
    SetAttributes(Scan_dir);
    
    //Set the initialization register
    InitReg();
}

/********************************************************************************
function:	Sets the start position and size of the display area
parameter:
		Xstart 	:   X direction Start coordinates
		Ystart  :   Y direction Start coordinates
		Xend    :   X direction end coordinates
		Yend    :   Y direction end coordinates
********************************************************************************/
void Waveshare128Round::SetWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend)
{
    //set the X coordinates
    SendCommand(0x2A);
    SendData_8Bit(0x00);
    SendData_8Bit(Xstart);
	SendData_8Bit((Xend-1)>>8);
    SendData_8Bit(Xend-1);

    //set the Y coordinates
    SendCommand(0x2B);
    SendData_8Bit(0x00);
	SendData_8Bit(Ystart);
	SendData_8Bit((Xend-1)>>8);
    SendData_8Bit(Yend-1);

    SendCommand(0X2C);
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void Waveshare128Round::Clear(UWORD Color)
{
    UWORD j;
    UWORD Image[LCD_1IN28_HEIGHT*LCD_1IN28_WIDTH];

    Color = ((Color<<8)&0xff00)|(Color>>8);

    for (j = 0; j < LCD_1IN28_HEIGHT*LCD_1IN28_WIDTH; j++) {
        Image[j] = Color;
    }
    
    SetWindows(0, 0, LCD_1IN28_WIDTH, LCD_1IN28_HEIGHT);
    DigitalWrite(LCD_DC_PIN, 1);
    for(j = 0; j < LCD_1IN28_HEIGHT; j++){
        writeSPI((uint8_t *)&Image[j*LCD_1IN28_WIDTH], LCD_1IN28_WIDTH*2);
    }
}

/******************************************************************************
function :	Sends the image buffer in RAM to displays
parameter:
******************************************************************************/
void Waveshare128Round::Display(UBYTE *Image)
{
    UWORD j, k;
    UWORD singleLine[LCD_1IN28_WIDTH];

    SetWindows(0, 0, LCD_1IN28_WIDTH, LCD_1IN28_HEIGHT);
    DigitalWrite(LCD_DC_PIN, 1);;
    for (j = 0; j < LCD_1IN28_HEIGHT; j++) {
        for (k = 0; k < LCD_1IN28_WIDTH; k++) {
            // need to turn our 8 bit color into 16 bit color
            singleLine[k] = (UWORD)(Image[k]);
        }
        writeSPI((UBYTE*)singleLine, LCD_1IN28_WIDTH*2);
    }
}

void Waveshare128Round::DisplayWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD *Image)
{
    // display
    UDOUBLE Addr = 0;

    UWORD j;
    SetWindows(Xstart, Ystart, Xend , Yend);
    DigitalWrite(LCD_DC_PIN, 1);;
    for (j = Ystart; j < Yend ; j++) {
        Addr = Xstart + j * LCD_1IN28_WIDTH ;
        writeSPI((uint8_t *)&Image[Addr], (Xend-Xstart)*2);
    }
}


void Waveshare128Round::DisplayPoint(UWORD X, UWORD Y, UWORD Color)
{
    SetWindows(X,Y,X,Y);
    SendData_16Bit(Color);
}
#endif