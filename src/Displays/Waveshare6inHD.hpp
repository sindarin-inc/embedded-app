#pragma once

#include <array>
#include <cstdint>
#include <driver/spi_master.h>
#include <stdbool.h>
#include <string>

#include "EinkTypes.hpp"
#include "Misc/Dispatch.hpp"

// This display is technically 1448x1072, but the IT8951 controller
// seems to want things 4 byte-aligned, so in their driver, they seem
// to sacrifice those 8 pixels on the right side. If we use this
// display more, we could probably investigate that further.
#define WAVESHARE_6INHD_X_PIXELS 1440
#define WAVESHARE_6INHD_Y_PIXELS 1072

#define HIGH 1
#define LOW 0

#define UBYTE uint8_t
#define UWORD uint16_t
#define UDOUBLE uint32_t

// INIT mode, for every init or some time after A2 mode refresh
extern UBYTE INIT_Mode;
// GC16 mode, for every time to display 16 grayscale image
extern UBYTE GC16_Mode;
// A2 mode, for fast refresh without flash
extern UBYTE A2_Mode;

typedef struct IT8951_Load_Img_Info_cpp {
    UWORD Endian_Type;          // little or Big Endian
    UWORD Pixel_Format;         // bpp
    UWORD Rotate;               // Rotate mode
    UBYTE *Source_Buffer_Addr;  // Start address of source Frame buffer
    UDOUBLE Target_Memory_Addr; // Base address of target image buffer
} IT8951_Load_Img_Info_cpp;

typedef struct IT8951_Area_Img_Info_cpp {
    UWORD Area_X;
    UWORD Area_Y;
    UWORD Area_W;
    UWORD Area_H;
} IT8951_Area_Img_Info_cpp;

typedef struct IT8951_Dev_Info_cpp {
    UWORD Panel_W;
    UWORD Panel_H;
    UWORD Memory_Addr_L;
    UWORD Memory_Addr_H;
    UWORD FW_Version[8];
    UWORD LUT_Version[8];
} IT8951_Dev_Info_cpp;

/*-----------------------------------------------------------------------
IT8951 Command defines
------------------------------------------------------------------------*/

// Built in I80 Command Code
#define IT8951_TCON_SYS_RUN 0x0001
#define IT8951_TCON_STANDBY 0x0002
#define IT8951_TCON_SLEEP 0x0003
#define IT8951_TCON_REG_RD 0x0010
#define IT8951_TCON_REG_WR 0x0011

#define IT8951_TCON_MEM_BST_RD_T 0x0012
#define IT8951_TCON_MEM_BST_RD_S 0x0013
#define IT8951_TCON_MEM_BST_WR 0x0014
#define IT8951_TCON_MEM_BST_END 0x0015

#define IT8951_TCON_LD_IMG 0x0020
#define IT8951_TCON_LD_IMG_AREA 0x0021
#define IT8951_TCON_LD_IMG_END 0x0022

// I80 User defined command code
#define USDEF_I80_CMD_DPY_AREA 0x0034
#define USDEF_I80_CMD_GET_DEV_INFO 0x0302
#define USDEF_I80_CMD_DPY_BUF_AREA 0x0037
#define USDEF_I80_CMD_VCOM 0x0039

/*-----------------------------------------------------------------------
 IT8951 Mode defines
------------------------------------------------------------------------*/

// Rotate mode
#define IT8951_ROTATE_0 0
#define IT8951_ROTATE_90 1
#define IT8951_ROTATE_180 2
#define IT8951_ROTATE_270 3

// Pixel mode (Bit per Pixel)
#define IT8951_2BPP 0
#define IT8951_3BPP 1
#define IT8951_4BPP 2
#define IT8951_8BPP 3

// Endian Type
#define IT8951_LDIMG_L_ENDIAN 0
#define IT8951_LDIMG_B_ENDIAN 1

/*-----------------------------------------------------------------------
IT8951 Registers defines
------------------------------------------------------------------------*/
// Register Base Address
#define DISPLAY_REG_BASE 0x1000 // Register RW access

// Base Address of Basic LUT Registers
#define LUT0EWHR (DISPLAY_REG_BASE + 0x00)  // LUT0 Engine Width Height Reg
#define LUT0XYR (DISPLAY_REG_BASE + 0x40)   // LUT0 XY Reg
#define LUT0BADDR (DISPLAY_REG_BASE + 0x80) // LUT0 Base Address Reg
#define LUT0MFN (DISPLAY_REG_BASE + 0xC0)   // LUT0 Mode and Frame number Reg
#define LUT01AF (DISPLAY_REG_BASE + 0x114)  // LUT0 and LUT1 Active Flag Reg

// Update Parameter Setting Register
#define UP0SR (DISPLAY_REG_BASE + 0x134)     // Update Parameter0 Setting Reg
#define UP1SR (DISPLAY_REG_BASE + 0x138)     // Update Parameter1 Setting Reg
#define LUT0ABFRV (DISPLAY_REG_BASE + 0x13C) // LUT0 Alpha blend and Fill rectangle Value
#define UPBBADDR (DISPLAY_REG_BASE + 0x17C)  // Update Buffer Base Address
#define LUT0IMXY (DISPLAY_REG_BASE + 0x180)  // LUT0 Image buffer X/Y offset Reg
#define LUTAFSR (DISPLAY_REG_BASE + 0x224)   // LUT Status Reg (status of All LUT Engines)
#define BGVR (DISPLAY_REG_BASE + 0x250)      // Bitmap (1bpp) image color table

// System Registers
#define SYS_REG_BASE 0x0000

// Address of System Registers
#define I80CPCR (SYS_REG_BASE + 0x04)

// Memory Converter Registers
#define MCSR_BASE_ADDR 0x0200
#define MCSR (MCSR_BASE_ADDR + 0x0000)
#define LISAR (MCSR_BASE_ADDR + 0x0008)

// Driver for dual Waveshare 6in HD 1448x1072 displays driven by IT8951 controller
class Waveshare6inHD {
public:
    // Constructor. Nothing interesting happens here until init()
    Waveshare6inHD(uint8_t cs, uint8_t rst) : cs_(cs), rst_(rst){};
    ~Waveshare6inHD() = default;

    UDOUBLE targetMem_ = 0;
    IT8951_Dev_Info_cpp DevInfo_;

    void init();

    // Vcom value is the drain voltage for the TFT FET.
    // It is written on back of each individual display
    void setVcom(int mV) { SetVCOM(mV); };

    // For the settings menu to display current value.
    auto getVcom() -> int { return vcomInMV_; }

    void setFramerate(int frameRate);

    void clear();

    void update(const uint8_t *buffer);

    auto errored() -> bool { return error_; }

    void Enhance_Driving_Capability(void);

    void SystemRun(void);

    void Standby(void);

    void Sleep(void);

    void Reset(void);

    IT8951_Dev_Info_cpp Init(UWORD VCOM);

    void Clear_Refresh(IT8951_Dev_Info_cpp Dev_Info, UDOUBLE Target_Memory_Addr, UWORD Mode);

    void One_bp_Refresh(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H, UBYTE Mode,
                        UDOUBLE Target_Memory_Addr, bool Packed_Write);
    void One_bp_Multi_Frame_Write(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H,
                                  UDOUBLE Target_Memory_Addr, bool Packed_Write);
    void One_bp_Multi_Frame_Refresh(UWORD X, UWORD Y, UWORD W, UWORD H, UDOUBLE Target_Memory_Addr);

    void Two_bp_Refresh(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H, bool Hold,
                        UDOUBLE Target_Memory_Addr, bool Packed_Write);

    void Four_bp_Refresh(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H, bool Hold,
                         UDOUBLE Target_Memory_Addr, bool Packed_Write);

    void Eight_bp_Refresh(UBYTE *Frame_Buf, UWORD X, UWORD Y, UWORD W, UWORD H, bool Hold,
                          UDOUBLE Target_Memory_Addr);

    void HostAreaPackedPixelWrite_1bp(IT8951_Load_Img_Info_cpp *info1,
                                      IT8951_Area_Img_Info_cpp *info2, bool packed);

    void HostAreaPackedPixelWrite_2bp(IT8951_Load_Img_Info_cpp *info1,
                                      IT8951_Area_Img_Info_cpp *info2, bool packed);

    void HostAreaPackedPixelWrite_4bp(IT8951_Load_Img_Info_cpp *info1,
                                      IT8951_Area_Img_Info_cpp *info2, bool packed);

    void HostAreaPackedPixelWrite_8bp(IT8951_Load_Img_Info_cpp *info1,
                                      IT8951_Area_Img_Info_cpp *info2);

    void Display_Area(UWORD X, UWORD Y, UWORD W, UWORD H, UWORD Mode);

    void Display_AreaBuf(UWORD X, UWORD Y, UWORD W, UWORD H, UWORD Mode,
                         UDOUBLE Target_Memory_Addr);

    void Display_1bp(UWORD X, UWORD Y, UWORD W, UWORD H, UWORD Mode, UDOUBLE Target_Memory_Addr,
                     UBYTE Back_Gray_Val, UBYTE Front_Gray_Val);

    uint16_t readTemperature(ScreenSide index) { return 0; };

protected:
    void inline ReadBusy();
    void WritePreamble();

    UWORD convertByte(UBYTE input);
    void WriteCommand(UWORD cmd);
    void inline WriteData(UWORD data);
    UWORD ReadData();
    void inline WriteMultiData(UWORD *data, UDOUBLE length);
    void ReadMultiData(UWORD *data, UDOUBLE length);
    void WriteMultiArg(UWORD arg, UWORD *buf, UWORD argNum);
    UWORD ReadReg(UWORD index);
    void WriteReg(UWORD index, UWORD value);

    UWORD GetVCOM();
    void SetVCOM(UWORD VCOM);

    void LoadImgStart(IT8951_Load_Img_Info_cpp *info);
    void LoadImgAreaStart(IT8951_Load_Img_Info_cpp *info1, IT8951_Area_Img_Info_cpp *info2);
    void LoadImgEnd();

    void GetSystemInfo(void *data);

    void SetTargetMemoryAddr(UDOUBLE Target_Memory_Addr);

    void WaitForDisplayReady();

    IT8951_Dev_Info_cpp InitDisplay(UWORD VCOM);

    IT8951_Dev_Info_cpp InitializeDriverChip();

    void powerOnInternal();
    void powerOffInternal();
    void inline writeRegister(const uint8_t *data, size_t len);
    auto readReg(UWORD index) -> UWORD;
    void inline SPIWriteByte(UBYTE Value);
    UBYTE SPIReadByte();

    inline void waitForNotBusy();
    void updateInternal(const uint8_t *buffer);

    uint8_t cs_;
    uint8_t rst_;

    // Keep track of both the vcom in the MV value (that is on the
    // sticker on the back of the display).
    // The 8-bit register value is calculated when it is set in the
    // register: ((5600 + vcomInMV_) * 255) / 5600.
    uint32_t vcomInMV_;

    uint16_t busyWaits_;
    uint32_t maxFrameCycles_;
    bool error_;

    //  uint8_t previousBuffer_[WAVESHARE_6INHD_X_PIXELS * WAVESHARE_6INHD_Y_PIXELS / 8];

    spi_device_handle_t spiHigh_;

    bool poweredOn_ = false;
    DispatchQueue *einkQueue_;
};