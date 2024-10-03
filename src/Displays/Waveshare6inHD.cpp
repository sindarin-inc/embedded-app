#if CONFIG_DISPLAY_WAVESHARE_6INHD

#include "Waveshare6inHD.hpp"

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

bool logging_ = false;

UBYTE INIT_Mode = 0;
// GC16 mode, for every time to display 16 grayscale image
UBYTE GC16_Mode = 2;
// A2 mode, for fast refresh without flash
UBYTE A2_Mode = 6;

#include <esp_heap_caps.h>
#include <time.h>

void Waveshare6inHD::update(const uint8_t *buffer) {}

/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
void Waveshare6inHD::Reset(void) {
    DigitalWrite(rst_, HIGH);
    Delay(200);
    DigitalWrite(rst_, LOW);
    Delay(10);
    DigitalWrite(rst_, HIGH);
    Delay(200);
}

void inline Waveshare6inHD::writeRegister(const uint8_t *data, size_t len) {
    spi_transaction_t t = {
        //.cmd = 0x70, // 0x70 is the command prefix
        .length = len * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    spi_device_polling_transmit(spiHigh_, &t);
}

/******************************************************************************
function:	SPI Write
parameter:
Info:
******************************************************************************/
void inline Waveshare6inHD::SPIWriteByte(UBYTE Value) {
    // DigitalWrite(cs_, LOW);
    // ESP_ERROR_CHECK(spi_device_acquire_bus(spiHigh_, portMAX_DELAY));
    writeRegister(&Value, 1);

    // spi_device_release_bus(spiHigh_);

    // DigitalWrite(cs_, HIGH);
    //  log_d("Writing out %d as value\n", Value);
    //  bcm2835_spi_transfer(Value);
}

/******************************************************************************
function:	SPI Read
parameter:
Info:
******************************************************************************/
UBYTE Waveshare6inHD::SPIReadByte() {
    UBYTE Read_Value = 0x10;

    // DigitalWrite(cs_, LOW);

    // ESP_ERROR_CHECK(spi_device_acquire_bus(spiLow_, portMAX_DELAY));
    spi_transaction_t t2 = {/*.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,*/
                            // .cmd = 0x73,
                            .length = 8,
                            .tx_buffer = NULL,
                            .rx_buffer = &Read_Value};
    spi_device_polling_transmit(spiHigh_, &t2);
    // Read_Value = t2.rx_data[0];
    // spi_device_release_bus(spiHigh_);

    // DigitalWrite(cs_, HIGH);

    // log_d("Got a read value of %d\n", Read_Value);
    // Read_Value = bcm2835_spi_transfer(0x00);
    return Read_Value;
}

/******************************************************************************
function :	Wait until the busy_pin goes HIGH
parameter:
******************************************************************************/
void inline Waveshare6inHD::ReadBusy(void) {
    UBYTE Busy_State = DigitalRead(CONFIG_DISPLAY_WAVESHARE_6INHD_BUSY);
    // 0: busy, 1: idle
    while (Busy_State == 0) {
        Busy_State = DigitalRead(CONFIG_DISPLAY_WAVESHARE_6INHD_BUSY);
    }
}

/******************************************************************************
function :	write command
parameter:  command
******************************************************************************/
void Waveshare6inHD::WriteCommand(UWORD Command) {
    // Set Preamble for Write Command
    UWORD Write_Preamble = 0x6000;
    ReadBusy();

    //    log_d("Writing command %d\n", Command);
    DigitalWrite(cs_, LOW);

    SPIWriteByte(Write_Preamble >> 8);
    SPIWriteByte(Write_Preamble);

    //  log_d("Wrote preamble");
    ReadBusy();

    SPIWriteByte(Command >> 8);
    SPIWriteByte(Command);

    // log_d("Wrote command");
    DigitalWrite(cs_, HIGH);
}

/******************************************************************************
function :	write data
parameter:  data
******************************************************************************/
void inline Waveshare6inHD::WriteData(UWORD Data) {
    // Set Preamble for Write Command
    UWORD Write_Preamble = 0x0000;

    ReadBusy();

    DigitalWrite(cs_, LOW);

    SPIWriteByte(Write_Preamble >> 8);
    SPIWriteByte(Write_Preamble);

    ReadBusy();

    SPIWriteByte(Data >> 8);
    SPIWriteByte(Data);

    DigitalWrite(cs_, HIGH);
}

/******************************************************************************
function :	write preamble
parameter:
******************************************************************************/
void Waveshare6inHD::WritePreamble() {
    // Set Preamble for Write Command
    UWORD Write_Preamble = 0x0000;

    ReadBusy();

    DigitalWrite(cs_, LOW);

    SPIWriteByte(Write_Preamble >> 8);
    SPIWriteByte(Write_Preamble);

    DigitalWrite(cs_, HIGH);
}

uint8_t inline reverse8Bits(uint8_t value) {
    uint8_t reversed = 0;
    for (int i = 0; i < 8; ++i) {
        if (value & (1 << i)) {
            reversed |= (1 << (7 - i));
        }
    }
    return reversed;
}

/******************************************************************************
function :	write multi data
parameter:  data
******************************************************************************/
void inline Waveshare6inHD::WriteMultiData(UWORD *Data_Buf, UDOUBLE Length) {
    // Set Preamble for Write Command
    UWORD Write_Preamble = 0x0000;

    ReadBusy();

    DigitalWrite(cs_, LOW);

    SPIWriteByte(Write_Preamble >> 8);
    SPIWriteByte(Write_Preamble);

    ReadBusy();

    size_t remaining = Length;
    size_t chunkSize = 32;

    for (UDOUBLE i = 0; i < Length && remaining > 0; i += chunkSize) {
        size_t len = 0;
        if (remaining < chunkSize) {
            len = remaining;
        } else {
            len = chunkSize;
        }

        uint16_t data[chunkSize];

        for (int j = 0; j < len; j++) {
            // Should change this to be a longer transaction to speed up the process
            // But each byte still needs to be reversed sadly.
            data[j] = reverse8Bits(Data_Buf[i + j]) | (reverse8Bits(Data_Buf[i + j] >> 8) << 8);
        }

        spi_transaction_t t = {
            //.cmd = 0x70, // 0x70 is the command prefix
            .length = len * 8 * 2,
            .tx_buffer = data,
            .rx_buffer = NULL,
        };

        spi_device_polling_transmit(spiHigh_, &t);
        remaining -= len;
    }
    DigitalWrite(cs_, HIGH);
}

/******************************************************************************
function :	read data
parameter:  data
******************************************************************************/
UWORD Waveshare6inHD::ReadData() {
    UWORD ReadData;
    UWORD Write_Preamble = 0x1000;
    UWORD Read_Dummy;

    ReadBusy();

    DigitalWrite(cs_, LOW);

    SPIWriteByte(Write_Preamble >> 8);
    SPIWriteByte(Write_Preamble);

    ReadBusy();

    // dummy
    Read_Dummy = SPIReadByte() << 8;
    Read_Dummy |= SPIReadByte();

    ReadBusy();

    ReadData = SPIReadByte() << 8;
    ReadData |= SPIReadByte();

    DigitalWrite(cs_, HIGH);

    return ReadData;
}

/******************************************************************************
function :	read multi data
parameter:  data
******************************************************************************/
void Waveshare6inHD::ReadMultiData(UWORD *Data_Buf, UDOUBLE Length) {
    UWORD Write_Preamble = 0x1000;
    UWORD Read_Dummy;

    ReadBusy();

    DigitalWrite(cs_, LOW);

    SPIWriteByte(Write_Preamble >> 8);
    SPIWriteByte(Write_Preamble);

    ReadBusy();

    // dummy
    Read_Dummy = SPIReadByte() << 8;
    Read_Dummy |= SPIReadByte();

    ReadBusy();

    for (UDOUBLE i = 0; i < Length; i++) {
        Data_Buf[i] = SPIReadByte() << 8;
        Data_Buf[i] |= SPIReadByte();
        // log_d("Found a data from read: %d\n", Data_Buf[i]);
    }

    DigitalWrite(cs_, HIGH);
}

/******************************************************************************
function:	write multi arg
parameter:	data
description:	some situation like this:
* 1 commander     0    argument
* 1 commander     1    argument
* 1 commander   multi  argument
******************************************************************************/
void Waveshare6inHD::WriteMultiArg(UWORD Arg_Cmd, UWORD *Arg_Buf, UWORD Arg_Num) {
    // Send Cmd code
    WriteCommand(Arg_Cmd);
    // Send Data
    for (UWORD i = 0; i < Arg_Num; i++) {
        //  into multi arg: %d", Arg_Buf[i]);
        // logging_ = true;
        WriteData(Arg_Buf[i]);
        // logging_ = false;
    }
}

/******************************************************************************
function :	Cmd4 ReadReg
parameter:
******************************************************************************/
UWORD Waveshare6inHD::ReadReg(UWORD Reg_Address) {
    UWORD Reg_Value;
    WriteCommand(IT8951_TCON_REG_RD);
    WriteData(Reg_Address);
    Reg_Value = ReadData();
    return Reg_Value;
}

/******************************************************************************
function :	Cmd5 WriteReg
parameter:
******************************************************************************/
void Waveshare6inHD::WriteReg(UWORD Reg_Address, UWORD Reg_Value) {
    WriteCommand(IT8951_TCON_REG_WR);
    WriteData(Reg_Address);
    WriteData(Reg_Value);
}

/******************************************************************************
function :	get VCOM
parameter:
******************************************************************************/
UWORD Waveshare6inHD::GetVCOM(void) {
    UWORD VCOM;
    WriteCommand(USDEF_I80_CMD_VCOM);
    WriteData(0x0000);
    VCOM = ReadData();
    return VCOM;
}

/******************************************************************************
function :	set VCOM
parameter:
******************************************************************************/
void Waveshare6inHD::SetVCOM(UWORD VCOM) {
    WriteCommand(USDEF_I80_CMD_VCOM);
    WriteData(0x0001);
    WriteData(VCOM);
}

/******************************************************************************
function :	Cmd10 LD_IMG
parameter:
******************************************************************************/
void Waveshare6inHD::LoadImgStart(IT8951_Load_Img_Info_cpp *Load_Img_Info) {
    UWORD Args;
    Args = (Load_Img_Info->Endian_Type << 8 | Load_Img_Info->Pixel_Format << 4 |
            Load_Img_Info->Rotate);
    WriteCommand(IT8951_TCON_LD_IMG);
    WriteData(Args);
}

/******************************************************************************
function :	Cmd11 LD_IMG_Area
parameter:
******************************************************************************/
void Waveshare6inHD::LoadImgAreaStart(IT8951_Load_Img_Info_cpp *Load_Img_Info,
                                      IT8951_Area_Img_Info_cpp *Area_Img_Info) {
    UWORD Args[5];
    Args[0] = (Load_Img_Info->Endian_Type << 8 | Load_Img_Info->Pixel_Format << 4 |
               Load_Img_Info->Rotate);
    Args[1] = Area_Img_Info->Area_X;
    Args[2] = Area_Img_Info->Area_Y;
    Args[3] = Area_Img_Info->Area_W;
    Args[4] = Area_Img_Info->Area_H;
    WriteMultiArg(IT8951_TCON_LD_IMG_AREA, Args, 5);
}

/******************************************************************************
function :	Cmd12 LD_IMG_End
parameter:
******************************************************************************/
void Waveshare6inHD::LoadImgEnd(void) { WriteCommand(IT8951_TCON_LD_IMG_END); }

/******************************************************************************
function :	Get_System_Info
parameter:
******************************************************************************/
void Waveshare6inHD::GetSystemInfo(void *Buf) {
    IT8951_Dev_Info_cpp *Dev_Info;
    // log_d("In GetSystemInfo");
    WriteCommand(USDEF_I80_CMD_GET_DEV_INFO);

    ReadMultiData((UWORD *)Buf, sizeof(IT8951_Dev_Info_cpp) / 2);

    Dev_Info = (IT8951_Dev_Info_cpp *)Buf;
    Dev_Info->Panel_W = WAVESHARE_6INHD_X_PIXELS;
    Dev_Info->Panel_H = WAVESHARE_6INHD_Y_PIXELS;
    // strcpy(Dev_Info->LUT_Version,  "M641");
    // log_d("Panel(W,H) = (%d,%d)\r\n", Dev_Info->Panel_W, Dev_Info->Panel_H);
    // log_d("Memory Address = %X\r\n", Dev_Info->Memory_Addr_L | (Dev_Info->Memory_Addr_H << 16));
    // log_d("FW Version = %s\r\n", (UBYTE *)Dev_Info->FW_Version);
    // log_d("LUT Version = %s\r\n", (UBYTE *)Dev_Info->LUT_Version);
    // log_d("Mem addr H %d, Mem addr low %d\n", Dev_Info->Memory_Addr_H, Dev_Info->Memory_Addr_L);
}

/******************************************************************************
function :	Set_Target_Memory_Addr
parameter:
******************************************************************************/
void Waveshare6inHD::SetTargetMemoryAddr(UDOUBLE Target_Memory_Addr) {
    UWORD WordH = (UWORD)((Target_Memory_Addr >> 16) & 0x0000FFFF);
    UWORD WordL = (UWORD)(Target_Memory_Addr & 0x0000FFFF);

    WriteReg(LISAR + 2, WordH);
    WriteReg(LISAR, WordL);
}

/******************************************************************************
function :	WaitForDisplayReady
parameter:
******************************************************************************/
void Waveshare6inHD::WaitForDisplayReady(void) {
    // Check IT8951 Register LUTAFSR => NonZero Busy, Zero - Free
    while (ReadReg(LUTAFSR)) {
        vTaskDelay(1);
        // wait in idle state
    }
}

UWORD Waveshare6inHD::convertByte(UBYTE input) {
    UWORD word = 0;
    for (int i = 0; i < 8; ++i) {
        if ((input >> i) & 1) {
            word |= 0b0011 << (i * 2);
        } else {
            word |= 0b0000 << (i * 2);
        }
    }

    // swap low and high bytes

    UBYTE low = word & 0x00FF;
    UBYTE high = (word & 0xFF00) >> 8;
    word = (low << 8) | high;
    return word;
}

uint16_t inline reverseBits(uint16_t value) {
    uint16_t reversed = 0;
    for (int i = 0; i < 16; ++i) {
        if (value & (1 << i)) {
            reversed |= (1 << (15 - i));
        }
    }
    return reversed;
}

/******************************************************************************
function :	EPD_IT8951_HostAreaPackedPixelWrite_1bp
parameter:
******************************************************************************/
void Waveshare6inHD::HostAreaPackedPixelWrite_1bp(IT8951_Load_Img_Info_cpp *Load_Img_Info,
                                                  IT8951_Area_Img_Info_cpp *Area_Img_Info,
                                                  bool Packed_Write) {
    UWORD Source_Buffer_Width, Source_Buffer_Height;
    UWORD Source_Buffer_Length;

    UWORD *Source_Buffer = (UWORD *)Load_Img_Info->Source_Buffer_Addr;
    SetTargetMemoryAddr(Load_Img_Info->Target_Memory_Addr);
    LoadImgAreaStart(Load_Img_Info, Area_Img_Info);

    // from byte to word
    // use 8bp to display 1bp, so here, divide by 2, because every byte has full bit.
    Source_Buffer_Width = Area_Img_Info->Area_W / 2;
    Source_Buffer_Height = Area_Img_Info->Area_H;
    Source_Buffer_Length = Source_Buffer_Width * Source_Buffer_Height;

    if (Packed_Write == true) {
        WriteMultiData(Source_Buffer, Source_Buffer_Length * 4);
    } else {
        for (UDOUBLE i = 0; i < Source_Buffer_Height; i++) {
            for (UDOUBLE j = 0; j < Source_Buffer_Width; j++) {
                WriteData(reverseBits(*Source_Buffer));
                Source_Buffer++;
            }
        }
    }

    LoadImgEnd();
}

/******************************************************************************
function :	HostAreaPackedPixelWrite_2bp
parameter:
******************************************************************************/
void Waveshare6inHD::HostAreaPackedPixelWrite_2bp(IT8951_Load_Img_Info_cpp *Load_Img_Info,
                                                  IT8951_Area_Img_Info_cpp *Area_Img_Info,
                                                  bool Packed_Write) {
    UWORD Source_Buffer_Width, Source_Buffer_Height;
    UWORD Source_Buffer_Length;

    UWORD *Source_Buffer = (UWORD *)Load_Img_Info->Source_Buffer_Addr;
    SetTargetMemoryAddr(Load_Img_Info->Target_Memory_Addr);
    LoadImgAreaStart(Load_Img_Info, Area_Img_Info);

    // from byte to word
    Source_Buffer_Width = (Area_Img_Info->Area_W * 2 / 8) / 2; // add /2
    Source_Buffer_Height = Area_Img_Info->Area_H;
    Source_Buffer_Length = Source_Buffer_Width * Source_Buffer_Height;

    if (Packed_Write == true) {
        WriteMultiData(Source_Buffer, Source_Buffer_Length);
    } else {
        for (UDOUBLE i = 0; i < Source_Buffer_Height; i++) {
            for (UDOUBLE j = 0; j < Source_Buffer_Width; j++) {
                WriteData(reverseBits(*Source_Buffer));
                Source_Buffer++;
            }
        }
    }

    LoadImgEnd();
}

/******************************************************************************
function :	HostAreaPackedPixelWrite_4bp
parameter:
******************************************************************************/
void Waveshare6inHD::HostAreaPackedPixelWrite_4bp(IT8951_Load_Img_Info_cpp *Load_Img_Info,
                                                  IT8951_Area_Img_Info_cpp *Area_Img_Info,
                                                  bool Packed_Write) {
    UWORD Source_Buffer_Width, Source_Buffer_Height;
    UWORD Source_Buffer_Length;

    UWORD *Source_Buffer = (UWORD *)Load_Img_Info->Source_Buffer_Addr;
    SetTargetMemoryAddr(Load_Img_Info->Target_Memory_Addr);
    LoadImgAreaStart(Load_Img_Info, Area_Img_Info);

    // from byte to word
    Source_Buffer_Width = (Area_Img_Info->Area_W * 4 / 8 / 2);
    Source_Buffer_Height = Area_Img_Info->Area_H;
    Source_Buffer_Length = Source_Buffer_Width * Source_Buffer_Height;

    if (Packed_Write == true) {
        WriteMultiData(Source_Buffer, Source_Buffer_Length);
    } else {
        for (UDOUBLE i = 0; i < Source_Buffer_Height; i++) {
            for (UDOUBLE j = 0; j < Source_Buffer_Width; j++) {
                WriteData(reverseBits(*Source_Buffer));
                Source_Buffer++;
            }
        }
    }

    LoadImgEnd();
}

/******************************************************************************
function :	HostAreaPackedPixelWrite_8bp
parameter:
Precautions: Can't Packed Write
******************************************************************************/
void Waveshare6inHD::HostAreaPackedPixelWrite_8bp(IT8951_Load_Img_Info_cpp *Load_Img_Info,
                                                  IT8951_Area_Img_Info_cpp *Area_Img_Info) {
    UWORD Source_Buffer_Width, Source_Buffer_Height;

    UWORD *Source_Buffer = (UWORD *)Load_Img_Info->Source_Buffer_Addr;
    SetTargetMemoryAddr(Load_Img_Info->Target_Memory_Addr);
    LoadImgAreaStart(Load_Img_Info, Area_Img_Info);

    // from byte to word
    Source_Buffer_Width = (Area_Img_Info->Area_W * 8 / 8) / 2; // add /2
    Source_Buffer_Height = Area_Img_Info->Area_H;

    UWORD chunksize = 32;
    for (UDOUBLE i = 0; i < Source_Buffer_Height; i++) {
        // For debugging purposes since this is a little slow
        // if (i % 100 == 0) {
        //     log_d("Wrote 100 lines %lu", i);
        // }
        // To optimize this, see WriteMultiData -- we could probably
        // do something like that here. I'm not sure why their driver doesn't
        // include the Packed option for 8bp like it does above for the rest.
        for (UDOUBLE j = 0; j < Source_Buffer_Width; j += chunksize) {
            UWORD len = 0;
            if (j + chunksize > Source_Buffer_Width) {
                len = Source_Buffer_Width - j;
            } else {
                len = chunksize;
            }
            WriteMultiData(Source_Buffer, len);
            Source_Buffer += len;
        }
    }

    LoadImgEnd();
}

/******************************************************************************
function :	Display_Area
parameter:
******************************************************************************/
void Waveshare6inHD::Display_Area(UWORD X, UWORD Y, UWORD W, UWORD H, UWORD Mode) {
    UWORD Args[5];
    Args[0] = X;
    Args[1] = Y;
    Args[2] = W;
    Args[3] = H;
    Args[4] = Mode;
    // 0x0034
    WriteMultiArg(USDEF_I80_CMD_DPY_AREA, Args, 5);
}

/******************************************************************************
function :	Display_AreaBuf
parameter:
******************************************************************************/
void Waveshare6inHD::Display_AreaBuf(UWORD X, UWORD Y, UWORD W, UWORD H, UWORD Mode,
                                     UDOUBLE Target_Memory_Addr) {
    UWORD Args[7];
    Args[0] = X;
    Args[1] = Y;
    Args[2] = W;
    Args[3] = H;
    Args[4] = Mode;
    Args[5] = (UWORD)Target_Memory_Addr;
    Args[6] = (UWORD)(Target_Memory_Addr >> 16);
    // 0x0037
    WriteMultiArg(USDEF_I80_CMD_DPY_BUF_AREA, Args, 7);
}

/******************************************************************************
function :	Display_1bp
parameter:
******************************************************************************/
void Waveshare6inHD::Display_1bp(UWORD X, UWORD Y, UWORD W, UWORD H, UWORD Mode,
                                 UDOUBLE Target_Memory_Addr, UBYTE Back_Gray_Val,
                                 UBYTE Front_Gray_Val) {
    // Set Display mode to 1 bpp mode - Set 0x18001138 Bit[18](0x1800113A Bit[2])to 1
    WriteReg(UP1SR + 2, ReadReg(UP1SR + 2) | (1 << 2));

    WriteReg(BGVR, (Front_Gray_Val << 8) | Back_Gray_Val);

    if (Target_Memory_Addr == 0) {
        Display_Area(X, Y, W, H, Mode);
    } else {
        Display_AreaBuf(X, Y, W, H, Mode, Target_Memory_Addr);
    }

    WaitForDisplayReady();

    WriteReg(UP1SR + 2, ReadReg(UP1SR + 2) & ~(1 << 2));
}

/******************************************************************************
function :	Enhanced driving capability
parameter:  Enhanced driving capability for IT8951, in case the blurred display effect
******************************************************************************/
void Waveshare6inHD::Enhance_Driving_Capability(void) {
    UWORD RegValue = ReadReg(0x0038);
    log_d("The reg value before writing is %x", RegValue);

    WriteReg(0x0038, 0x0602);

    RegValue = ReadReg(0x0038);
    log_d("The reg value after writing is %x", RegValue);
}

/******************************************************************************
function :	Cmd1 SYS_RUN
parameter:  Run the system
******************************************************************************/
void Waveshare6inHD::SystemRun(void) { WriteCommand(IT8951_TCON_SYS_RUN); }

/******************************************************************************
function :	Cmd2 STANDBY
parameter:  Standby
******************************************************************************/
void Waveshare6inHD::Standby(void) { WriteCommand(IT8951_TCON_STANDBY); }

/******************************************************************************
function :	Cmd3 SLEEP
parameter:  Sleep
******************************************************************************/
void Waveshare6inHD::Sleep(void) { WriteCommand(IT8951_TCON_SLEEP); }

/******************************************************************************
function :	Init
parameter:
******************************************************************************/
IT8951_Dev_Info_cpp Waveshare6inHD::InitDisplay(UWORD VCOM) {
    IT8951_Dev_Info_cpp Dev_Info;
    Reset();
    SystemRun();
    GetSystemInfo(&Dev_Info);
    targetMem_ = Dev_Info.Memory_Addr_L | (Dev_Info.Memory_Addr_H << 16);

    //  Enable Pack write
    WriteReg(I80CPCR, 0x0001);

    //  Set VCOM by handle
    if (VCOM != GetVCOM()) {
        SetVCOM(VCOM);
    }

    Clear_Refresh(Dev_Info, targetMem_, INIT_Mode);

    return Dev_Info;
}

/******************************************************************************
function :	Clear_Refresh
parameter:
******************************************************************************/
void Waveshare6inHD::Clear_Refresh(IT8951_Dev_Info_cpp Dev_Info, UDOUBLE Target_Memory_Addr,
                                   UWORD Mode) {
    UDOUBLE ImageSize = ((Dev_Info.Panel_W * 4 % 8 == 0) ? (Dev_Info.Panel_W * 4 / 8)
                                                         : (Dev_Info.Panel_W * 4 / 8 + 1)) *
                        Dev_Info.Panel_H;
    UBYTE *Frame_Buf = (UBYTE *)heap_caps_malloc(ImageSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    memset(Frame_Buf, 0xFF, ImageSize);

    IT8951_Load_Img_Info_cpp Load_Img_Info;
    IT8951_Area_Img_Info_cpp Area_Img_Info;

    WaitForDisplayReady();
    Load_Img_Info.Source_Buffer_Addr = Frame_Buf;
    Load_Img_Info.Endian_Type = IT8951_LDIMG_B_ENDIAN;
    Load_Img_Info.Pixel_Format = IT8951_4BPP;
    Load_Img_Info.Rotate = IT8951_ROTATE_0;
    Load_Img_Info.Target_Memory_Addr = Target_Memory_Addr;

    Area_Img_Info.Area_X = 0;
    Area_Img_Info.Area_Y = 0;
    Area_Img_Info.Area_W = Dev_Info.Panel_W;
    Area_Img_Info.Area_H = Dev_Info.Panel_H;
    // Their driver has this as false, but true is way faster. (20 seconds with false, ~1 second
    // with true)
    HostAreaPackedPixelWrite_4bp(&Load_Img_Info, &Area_Img_Info, true);
    Display_Area(0, 0, Dev_Info.Panel_W, Dev_Info.Panel_H, Mode);
    free(Frame_Buf);
    Frame_Buf = NULL;
}

/******************************************************************************
function :	One_bp_Refresh
parameter:
******************************************************************************/
void Waveshare6inHD::One_bp_Refresh(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H,
                                    UBYTE Mode, UDOUBLE Target_Memory_Addr, bool Packed_Write) {
    IT8951_Load_Img_Info_cpp Load_Img_Info;
    IT8951_Area_Img_Info_cpp Area_Img_Info;

    WaitForDisplayReady();

    Load_Img_Info.Source_Buffer_Addr = Frame_Buf;
    Load_Img_Info.Endian_Type = IT8951_LDIMG_B_ENDIAN; // IT8951_LDIMG_L_ENDIAN;
    // Use 8bpp to set One_bpp
    Load_Img_Info.Pixel_Format = IT8951_8BPP;
    Load_Img_Info.Rotate = IT8951_ROTATE_0;
    if (Target_Memory_Addr == 0) {
        Target_Memory_Addr = targetMem_;
    }
    Load_Img_Info.Target_Memory_Addr = Target_Memory_Addr;

    Area_Img_Info.Area_X = X / 8;
    Area_Img_Info.Area_Y = Y;
    Area_Img_Info.Area_W = W / 8;
    Area_Img_Info.Area_H = H;

    HostAreaPackedPixelWrite_1bp(&Load_Img_Info, &Area_Img_Info, Packed_Write);

    Display_1bp(X, Y, W, H, Mode, Target_Memory_Addr, 0xF0, 0x00);
}

/******************************************************************************
function :	1bp_Multi_Frame_Write
parameter:
******************************************************************************/
void Waveshare6inHD::One_bp_Multi_Frame_Write(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H,
                                              UDOUBLE Target_Memory_Addr, bool Packed_Write) {
    IT8951_Load_Img_Info_cpp Load_Img_Info;
    IT8951_Area_Img_Info_cpp Area_Img_Info;

    WaitForDisplayReady();

    Load_Img_Info.Source_Buffer_Addr = Frame_Buf;
    Load_Img_Info.Endian_Type = IT8951_LDIMG_B_ENDIAN;
    // Use 8bpp to set 1bpp
    Load_Img_Info.Pixel_Format = IT8951_8BPP;
    Load_Img_Info.Rotate = IT8951_ROTATE_0;
    Load_Img_Info.Target_Memory_Addr = Target_Memory_Addr;

    Area_Img_Info.Area_X = X / 8;
    Area_Img_Info.Area_Y = Y;
    Area_Img_Info.Area_W = W / 8;
    Area_Img_Info.Area_H = H;

    HostAreaPackedPixelWrite_2bp(&Load_Img_Info, &Area_Img_Info, Packed_Write);
}

/******************************************************************************
function :	1bp_Multi_Frame_Refresh
parameter:
******************************************************************************/
void Waveshare6inHD::One_bp_Multi_Frame_Refresh(UWORD X, UWORD Y, UWORD W, UWORD H,
                                                UDOUBLE Target_Memory_Addr) {
    WaitForDisplayReady();

    Display_1bp(X, Y, W, H, A2_Mode, Target_Memory_Addr, 0xF0, 0x00);
}

/******************************************************************************
function :	2bp_Refresh
parameter:
******************************************************************************/
void Waveshare6inHD::Two_bp_Refresh(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H, bool Hold,
                                    UDOUBLE Target_Memory_Addr, bool Packed_Write) {
    IT8951_Load_Img_Info_cpp Load_Img_Info;
    IT8951_Area_Img_Info_cpp Area_Img_Info;

    WaitForDisplayReady();

    Load_Img_Info.Source_Buffer_Addr = Frame_Buf;
    Load_Img_Info.Endian_Type = IT8951_LDIMG_B_ENDIAN;
    Load_Img_Info.Pixel_Format = IT8951_2BPP;
    Load_Img_Info.Rotate = IT8951_ROTATE_0;
    if (Target_Memory_Addr == 0) {
        Target_Memory_Addr = targetMem_;
    }
    Load_Img_Info.Target_Memory_Addr = Target_Memory_Addr;

    Area_Img_Info.Area_X = X;
    Area_Img_Info.Area_Y = Y;
    Area_Img_Info.Area_W = W;
    Area_Img_Info.Area_H = H;

    HostAreaPackedPixelWrite_2bp(&Load_Img_Info, &Area_Img_Info, Packed_Write);

    if (Hold == true) {
        Display_Area(X, Y, W, H, GC16_Mode);
    } else {
        Display_AreaBuf(X, Y, W, H, GC16_Mode, Target_Memory_Addr);
    }
}

/******************************************************************************
function :	4bp_Refresh
parameter:
******************************************************************************/
void Waveshare6inHD::Four_bp_Refresh(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H,
                                     bool Hold, UDOUBLE Target_Memory_Addr, bool Packed_Write) {
    IT8951_Load_Img_Info_cpp Load_Img_Info;
    IT8951_Area_Img_Info_cpp Area_Img_Info;

    WaitForDisplayReady();

    Load_Img_Info.Source_Buffer_Addr = Frame_Buf;
    Load_Img_Info.Endian_Type = IT8951_LDIMG_B_ENDIAN;
    Load_Img_Info.Pixel_Format = IT8951_4BPP;
    Load_Img_Info.Rotate = IT8951_ROTATE_0;

    if (Target_Memory_Addr == 0) {
        Target_Memory_Addr = targetMem_;
    }
    Load_Img_Info.Target_Memory_Addr = Target_Memory_Addr;

    Area_Img_Info.Area_X = X;
    Area_Img_Info.Area_Y = Y;
    Area_Img_Info.Area_W = W;
    Area_Img_Info.Area_H = H;

    HostAreaPackedPixelWrite_4bp(&Load_Img_Info, &Area_Img_Info, Packed_Write);

    if (Hold == true) {
        Display_Area(X, Y, W, H, GC16_Mode);
    } else {
        Display_AreaBuf(X, Y, W, H, GC16_Mode, Target_Memory_Addr);
    }
}

/******************************************************************************
function :	Eight_bp_Refresh
parameter:
******************************************************************************/
void Waveshare6inHD::Eight_bp_Refresh(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H,
                                      bool Hold, UDOUBLE Target_Memory_Addr) {
    IT8951_Load_Img_Info_cpp Load_Img_Info;
    IT8951_Area_Img_Info_cpp Area_Img_Info;

    WaitForDisplayReady();

    Load_Img_Info.Source_Buffer_Addr = Frame_Buf;
    Load_Img_Info.Endian_Type = IT8951_LDIMG_B_ENDIAN;
    Load_Img_Info.Pixel_Format = IT8951_8BPP;
    Load_Img_Info.Rotate = IT8951_ROTATE_0;

    if (Target_Memory_Addr == 0) {
        Target_Memory_Addr = targetMem_;
    }
    Load_Img_Info.Target_Memory_Addr = Target_Memory_Addr;

    Area_Img_Info.Area_X = X;
    Area_Img_Info.Area_Y = Y;
    Area_Img_Info.Area_W = W;
    Area_Img_Info.Area_H = H;

    HostAreaPackedPixelWrite_8bp(&Load_Img_Info, &Area_Img_Info);

    if (Hold == true) {
        Display_Area(X, Y, W, H, GC16_Mode);
    } else {
        Display_AreaBuf(X, Y, W, H, GC16_Mode, Target_Memory_Addr);
    }
}

void Waveshare6inHD::init() {
    log_d("Initializing Waveshare6inHD with config cs_=%d rst_=%d mosi=%d miso=%d "
          "clk=%d",
          cs_, rst_, CONFIG_DISPLAY_WAVESHARE_6INHD_MOSI, CONFIG_DISPLAY_WAVESHARE_6INHD_MISO,
          CONFIG_DISPLAY_WAVESHARE_6INHD_CLK);
    PinMode(rst_, OUTPUT);
    PinMode(cs_, OUTPUT);

#ifdef CONFIG_DISPLAY_WAVESHARE_6INHD_BUSY
    log_d("Using CONFIG_DISPLAY_WAVESHARE_6INHD_BUSY pin %d", CONFIG_DISPLAY_WAVESHARE_6INHD_BUSY);
    PinMode(CONFIG_DISPLAY_WAVESHARE_6INHD_BUSY, INPUT);
#endif

    DigitalWrite(cs_, HIGH);

    spi_bus_config_t buscfg = {
        .mosi_io_num = CONFIG_DISPLAY_WAVESHARE_6INHD_MOSI,
        .miso_io_num = CONFIG_DISPLAY_WAVESHARE_6INHD_MISO,
        .sclk_io_num = CONFIG_DISPLAY_WAVESHARE_6INHD_CLK,
        // These must be unset or the SPI driver will try to use them
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };

    esp_err_t err = spi_bus_initialize(CONFIG_DISPLAY_SPI, &buscfg, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        log_e("Error initializing SPI bus %d", err);
        return;
    }

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .mode = 0,

        .clock_source = SPI_CLK_SRC_APB,
        .clock_speed_hz = 20 * 1000 * 1000,

        .spics_io_num = -1,
        .queue_size = 1,
    };

    err = spi_bus_add_device(CONFIG_DISPLAY_SPI, &devcfg, &spiHigh_);
    if (err != ESP_OK) {
        log_e("Error adding display device to SPI bus: %d", err);
    }

    DevInfo_ = InitDisplay(2080);
}

#endif