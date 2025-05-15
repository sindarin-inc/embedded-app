/*
 * SPDX-FileCopyrightText: 2025 Sindarin Inc.
 *
 * Driver for SeeYA SY049WDM07 (0.49" micro-OLED, 1920x1080) via MIPI DSI
 */

#pragma once

#include <memory.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEEYA049_I2C_SHARED_ADDR 0x4E
// RL_SEL = 0
#define SEEYA049_I2C_PRIMARY_ADDR 0x4C
// RL_SEL = 1
#define SEEYA049_I2C_SECONDARY_ADDR 0x4D

// From the SY103WAM01 datasheet
// #define SEEYA049_CMD_SWRESET 0x01 // Software Reset
// #define SEEYA049_CMD_CMODE 0x03   // Compression Mode
// #define SEEYA049_CMD_SLPIN 0x10   // Sleep In
// #define SEEYA049_CMD_SLPOUT 0x11  // Sleep Out
// #define SEEYA049_CMD_ALLPOFF 0x22 // All Display Off
// #define SEEYA049_CMD_ALLPON 0x23  // All Display On
// #define SEEYA049_CMD_TCON 0x25    // Temperature Sensor Enable
// #define SEEYA049_CMD_DISPOFF 0x28 // Display Off
// #define SEEYA049_CMD_DISPON 0x29  // Display On
// #define SEEYA049_CMD_CASET 0x2A   // Column Address Set
// #define SEEYA049_CMD_RASET 0x2B   // Row Address Set
// #define SEEYA049_CMD_TEON 0x2C    // Tearing Effect Line On
// #define SEEYA049_CMD_MADCTL 0x36    // Memory Access Control
// #define SEEYA049_CMD_IDMOFF 0x38    // Idle Mode Off
// #define SEEYA049_CMD_IDMON 0x39     // Idle Mode On
// #define SEEYA049_CMD_COLMOD 0x3A    // Interface Pixel Format
// #define SEEYA049_CMD_WRDISBV 0x51 // Write Display Brightness
#define SEEYA049_CMD_WRCTRLD 0x53 // Write CTRL Display
// #define SEEYA049_CMD_SCACTRL 0x69   // Scaling Up Control
// #define SEEYA049_CMD_IFCONFG 0x6B   // Interface Configure
#define SEEYA049_CMD_RESCTRL1 0x80 // Resolution Control1
#define SEEYA049_CMD_RESCTRL2 0x81 // Resolution Control2
#define SEEYA049_CMD_RESCTRL3 0x82 // Resolution Control3
// #define SEEYA049_CMD_PORCH_EXT 0x83 // Porch CTRL

// From the SY103WDM07 datasheet
#define SEEYA049_CMD_SWRESET 0x01 // Software Reset
#define SEEYA049_CMD_CMODE 0x03   // Compression Mode
#define SEEYA049_CMD_SLPIN 0x10   // Sleep In
#define SEEYA049_CMD_SLPOUT 0x11  // Sleep Out
#define SEEYA049_CMD_ALLPOFF 0x22 // All Pixels Off
#define SEEYA049_CMD_ALLPON 0x23  // All Pixels On
#define SEEYA049_CMD_DISPOFF 0x28 // Display Off
#define SEEYA049_CMD_DISPON 0x29  // Display On
#define SEEYA049_CMD_CASET 0x2A   // Column Address Set
#define SEEYA049_CMD_RASET 0x2B   // Row Address Set
// According to SeeYa, this is a small memory buffer for an OSD, but it doesn't work in the 0.49"
// screens.
#define SEEYA049_CMD_RAMWR 0x2C // Memory Write
#define SEEYA049_CMD_RAMRD 0x2E // Memory Read
// Note: the RGB bit in MADCTL doesn't seem to have an effect
#define SEEYA049_CMD_MADCTL 0x36         // Memory Access Control
#define SEEYA049_CMD_MADCTL_RGB (1 << 3) // Color-order of sub-pixels (doesn't seem to work)
#define SEEYA049_CMD_MADCTL_MY (1 << 0)  // Mirror Y
#define SEEYA049_CMD_MADCTL_MX (1 << 1)  // Mirror X
#define SEEYA049_CMD_IDMOFF 0x38         // Idle Mode Off
#define SEEYA049_CMD_IDMON 0x39          // Idle Mode On
// Scaling seems to work but causes some flickering
#define SEEYA049_CMD_SCACTRL 0x69 // Scaling Up Control
#define SEEYA049_CMD_IFCONFG 0x6B // Interface Configure (lane count)
#define SEEYA049_CMD_ANCTRL1 0x90 // Display Anchor On/Off
#define SEEYA049_CMD_ANCTRL2 0x91 // Display Anchor CTRL
#define SEEYA049_CMD_BCCTR3 0xC2  // Brightness Control

// From the SY103WDM01 datasheet (only the commands that are different from SY103WDM07)
// COLMOD is only documented as valid as 0x77 and doesn't seem to have an effect
#define SEEYA049_CMD_COLMOD 0x3A  // Interface Pixel Format
#define SEEYA049_CMD_WRDISBV 0x51 // Write Display Brightness

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    uint16_t cmd;          /*<! The specific LCD command */
    const void *data;      /*<! Buffer that holds the command specific data */
    size_t data_bytes;     /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms; /*<! Delay in milliseconds after this command */
} seeya049_lcd_init_cmd_t;

// I2C-specific init sequence
static const seeya049_lcd_init_cmd_t seeya049_init_cmds_i2c[] = {
    // CMD1
    {.cmd = SEEYA049_CMD_WRCTRLD, .data = (uint8_t[]){0x29}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_WRDISBV, .data = (uint8_t[]){0xFF, 0x01}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_CMODE, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_SCACTRL, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_IFCONFG, .data = (uint8_t[]){0x02}, .data_bytes = 1, .delay_ms = 0},
    // RESCTL1
    {.cmd = SEEYA049_CMD_RESCTRL1,
     // If this is like the 1.03" datasheet, this command can affect the screen resolution
     // Full resolution 1920x1080 (0x1E0*4 = 1920, 0x10E*4 = 1080)
     .data = (uint8_t[]){0x00, 0xE0, 0xE0, 0x0E, 0x00, 0x31},
     // Half resolution 960x540 (0xF0*4 = 960, 0x87*4 = 540)
     //  .data = (uint8_t[]){0x00, 0xF0, 0xF0, 0x87, 0x00, 0x10},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RESCTRL2,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RESCTRL3,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    // NOTE(john): I've added these three commands, though I'm not entirely sure this is the
    // right
    // place in the sequence
    {.cmd = SEEYA049_CMD_CASET, .data = (uint8_t[]){0x00, 0x00}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RASET, .data = (uint8_t[]){0x00, 0x00}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_MADCTL, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x35, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x26, .data = (uint8_t[]){0x20}, .data_bytes = 1, .delay_ms = 0},

    // CMD2 P1
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_BCCTR3,
     .data = (uint8_t[]){0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF},
     .data_bytes = 8,
     .delay_ms = 0},

    // CMD2,P0
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x12}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x37, 0xA9}, .data_bytes = 2, .delay_ms = 0},

    // CMD3 P0
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x80}, .data_bytes = 2, .delay_ms = 0},
    // Why such a weird register address?
    {.cmd = 0xF22F, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 0},

    // CMD3 P1
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x81}, .data_bytes = 2, .delay_ms = 0},
    // Why such a weird register address?
    {.cmd = 0xF205, .data = (uint8_t[]){0x22}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF20A, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF917,
     .data =
         (uint8_t[]){0x5E, 0x62, 0x66, 0x6A, 0x6F, 0x73, 0x77, 0x7B, 0x7F, 0x84, 0x88, 0x8C, 0x90},
     .data_bytes = 13,
     .delay_ms = 0},

    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC0, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 20},
    {.cmd = 0x11, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 100},
    {.cmd = 0x29, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 20},
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC0, .data = (uint8_t[]){0xFF}, .data_bytes = 1, .delay_ms = 0},
};

// When RL_SEL is 1.8V these need to be set. But we don't send them by default.
static const seeya049_lcd_init_cmd_t seeya049_init_cmds_i2c_rl_sel[] = {
    {.cmd = SEEYA049_CMD_MADCTL, .data = (uint8_t[]){0x03}, .data_bytes = 1, .delay_ms = 0},
    // Command2 Page3
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x13}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC101, .data = (uint8_t[]){0x22}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xC406,
     .data = (uint8_t[]){0x31, 0x42, 0x56, 0x12, 0x53, 0x64},
     .data_bytes = 6,
     .delay_ms = 0},
    // Command2 Page6
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x16}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xB606,
     .data = (uint8_t[]){0x31, 0x42, 0x56, 0x12, 0x53, 0x64},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = 0xB0, .data = (uint8_t[]){0x00, 0x44}, .data_bytes = 2, .delay_ms = 0},
};

// NED-provided init sequence
// The only critical command from NED seems to be SEEYA049_CMD_WRCTRLD. Then we add
// SEEYA049_CMD_IFCONFG to make the display work in 2 lane mode.
static const seeya049_lcd_init_cmd_t seeya049_init_cmds_mipi[] = {
    // CMD1
    // WRCTRLD? Absolutely critical -- this must happen or nothing else works
    // Based on the SEEYA_SY103WAM01_Pre-spec V0.4.pdf datasheet here's what the bits mean:
    // D5: Brightness control enable (must be on)
    // D3: Display dimming control enable
    // D2: Temperature index dimming control enable
    // D0: Display mode control (1=High brightness, low contrast, 0=Low brightness, high
    // contrast)
    {.cmd = SEEYA049_CMD_WRCTRLD, .data = (uint8_t[]){0x29}, .data_bytes = 1, .delay_ms = 0},

    // WRDISBV? Display brightness control? No change when removing
    {.cmd = SEEYA049_CMD_WRDISBV, .data = (uint8_t[]){0xFF, 0x01}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_CMODE, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_SCACTRL, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_IFCONFG, .data = (uint8_t[]){0x02}, .data_bytes = 1, .delay_ms = 0},
    // RESCTL1
    {.cmd = SEEYA049_CMD_RESCTRL1,
     // If this is like the 1.03" datasheet, this command can affect the screen resolution
     //  Full resolution 1920x1080 (0x1E0*4 = 1920, 0x10E*4 = 1080)
     //  .data = (uint8_t[]){0x00, 0xE0, 0xE0, 0x0E, 0x00, 0x31},
     // Half resolution 960x540 (0xF0*4 = 960, 0x87*4 = 540)
     //  .data = (uint8_t[]){0x00, 0xF0, 0xE0, 0x87, 0x00, 0x10},
     // The first byte selects the faster internal oscillator frequency and was necessary to set
     // for 90Hz refresh with 2x scaling.
     .data = (uint8_t[]){0x00, 0xE0, 0xE0, 0x0E, 0x00, 0x31},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RESCTRL2,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RESCTRL3,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = SEEYA049_CMD_CASET, .data = (uint8_t[]){0x00, 0x00}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RASET, .data = (uint8_t[]){0x00, 0x00}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_MADCTL, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    // Tearing effect line on
    {.cmd = 0x35, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    // Gamma set
    {.cmd = 0x26, .data = (uint8_t[]){0x20}, .data_bytes = 1, .delay_ms = 0},

    // CMD2 P1
    // No change in behavior removing these
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC0, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_BCCTR3,
     .data = (uint8_t[]){0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF},
     .data_bytes = 8,
     .delay_ms = 0},

    // CMD2 P2
    // No change removing these
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x12}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x37, 0xA9}, .data_bytes = 2, .delay_ms = 0},

    // CMD3 P0
    // No change removing these
    // Darin suggested moving these to get 90Hz refresh rate with 2x scaling, but they didn't seem
    // to make a difference
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x80}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x2F}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 0},

    // CMD3 P1
    // No change removing these
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x81}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x05}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x22}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x0A}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x17}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF9,
     .data =
         (uint8_t[]){0x5E, 0x62, 0x66, 0x6A, 0x6F, 0x73, 0x77, 0x7B, 0x7F, 0x84, 0x88, 0x8C, 0x90},
     .data_bytes = 13,
     .delay_ms = 0},

    // Final configuration
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC5, .data = (uint8_t[]){0x00, 0x08}, .data_bytes = 2, .delay_ms = 0},

    // Wake from sleep
    {.cmd = 0, .data = NULL, .data_bytes = 0, .delay_ms = 20},
    {.cmd = SEEYA049_CMD_SLPOUT, .data = NULL, .data_bytes = 0, .delay_ms = 100}, // Sleep Out
};

// Built-in self-test init sequence provided by NED
static const seeya049_lcd_init_cmd_t seeya049_init_cmds_selftest[] = {
    // CMD1
    {.cmd = SEEYA049_CMD_WRCTRLD, .data = (uint8_t[]){0x29}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_WRDISBV, .data = (uint8_t[]){0xFF, 0x01}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_CMODE, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RESCTRL1,
     .data = (uint8_t[]){0x00, 0xE0, 0xE0, 0x0E, 0x00, 0x31},
     // Half-resolution 960x540 (0xF0*4 = 960, 0x87*4 = 540)
     //  .data = (uint8_t[]){0x00, 0xF0, 0xF0, 0x87, 0x00, 0x10},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RESCTRL2,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = SEEYA049_CMD_RESCTRL3,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x01},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = 0x35, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x26, .data = (uint8_t[]){0x20}, .data_bytes = 1, .delay_ms = 0},

    // CMD2 P1
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC0, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_BCCTR3,
     .data = (uint8_t[]){0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF},
     .data_bytes = 8,
     .delay_ms = 0},

    // CMD2 P2
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x12}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x37, 0xA9}, .data_bytes = 2, .delay_ms = 0},

    // CMD3 P0
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x80}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x2F}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 0},

    // CMD3 P1
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x81}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x05}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x22}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x0A}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x17}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF9,
     .data =
         (uint8_t[]){0x5E, 0x62, 0x66, 0x6A, 0x6F, 0x73, 0x77, 0x7B, 0x7F, 0x84, 0x88, 0x8C, 0x90},
     .data_bytes = 13,
     .delay_ms = 0},

    // Final configuration
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC5,
     .data = (uint8_t[]){0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = 0xC4, .data = (uint8_t[]){0xAA, 0x55, 0x01, 0x80}, .data_bytes = 4, .delay_ms = 0},
};

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_vendor.h"

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the
 * `esp_lcd_panel_dev_config_t::vendor_config`.
 *
 */

typedef enum {
    SEEYA049_SCALING_NONE = 0,
    SEEYA049_SCALING_1_33X = 1,
    SEEYA049_SCALING_1_5X = 2,
    SEEYA049_SCALING_2X = 3,
} seeya049_scaling_t;

typedef struct {
    const seeya049_lcd_init_cmd_t
        *init_cmds;          /*!< Pointer to initialization commands array. Set to NULL if using
                              * default commands. The array should be declared as `static const`
                              * and positioned outside the function. Please refer to
                              * `seeya049_init_cmds_mipi` in source file.
                              */
    uint16_t init_cmds_size; /*!< Number of commands in above array */
    uint16_t refresh_rate; /*!< Target refresh rate of the display. If set this will be used to help
                              calculate the dpi_clock_freq_mhz */
    uint16_t x_offset;     /*!< X offset of the display */
    uint16_t y_offset;     /*!< Y offset of the display */
    seeya049_scaling_t scaling;
    struct {
        esp_lcd_dsi_bus_handle_t dsi_bus;             /*!< MIPI-DSI bus configuration */
        const esp_lcd_dpi_panel_config_t *dpi_config; /*!< MIPI-DPI panel configuration */
        uint8_t lane_num;                             /*!< Number of MIPI-DSI lanes */
    } mipi_config;
} seeya049_vendor_config_t;

/**
 * @brief Create LCD panel for model SeeYA SY049WDM07
 *
 * @note  Vendor specific initialization can be different between manufacturers, should
 * consult the LCD supplier for initialization sequence code.
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config General panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG   if parameter is invalid
 *      - ESP_OK                on success
 *      - Otherwise             on fail
 */
esp_err_t esp_lcd_new_panel_seeya049(const esp_lcd_panel_io_handle_t io,
                                     const esp_lcd_panel_dev_config_t *panel_dev_config,
                                     esp_lcd_panel_handle_t *ret_panel);

esp_err_t panel_seeya049_send_init_cmds(esp_lcd_panel_t *panel);

esp_err_t panel_seeya049_all_pixels_on_off(esp_lcd_panel_t *panel, bool on_off);

/**
 * @brief Run the panel's built-in self test mode initialization sequence
 *
 * @param[in] panel LCD panel handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_run_selftest(esp_lcd_panel_t *panel);

/**
 * @brief Write raw data to the display RAM
 *
 * @param[in] panel LCD panel handle
 * @param[in] data Pointer to the data buffer to write
 * @param[in] length Length of data in bytes
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_write_ram(esp_lcd_panel_t *panel, const void *data, size_t length);

/**
 * @brief Dump the display's framebuffer over I2C using the RAMRD command
 *
 * This function will read the entire framebuffer (1920x1080x3 bytes) and print
 * the first 16 bytes for debugging purposes.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NO_MEM if memory allocation fails
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_dump_framebuffer_i2c(void);

/**
 * @brief Move the display area of the panel
 *
 * @param[in] panel LCD panel handle
 * @param[in] dx Delta x
 * @param[in] dy Delta y
 */
esp_err_t panel_seeya049_move_display_area(esp_lcd_panel_t *panel, int16_t dx, int16_t dy);

static inline esp_lcd_dsi_bus_config_t panel_seeya049_dsi_bus_config(void) {
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        // This doesn't seem to affect the size of what is drawn
        // lane_bit_rate_mbps / 8 / 18 must be > 1 and < 256
        .lane_bit_rate_mbps = 1000,
    };
    return bus_config;
}

static inline esp_lcd_dbi_io_config_t panel_seeya049_io_dbi_config(void) {
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 16,
    };
    return dbi_config;
}

static inline esp_lcd_dpi_panel_config_t panel_seeya049_1920_1080_dpi_config(void) {
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 0, // Set by the refresh rate
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB888,
        .in_color_format = (lcd_color_format_t)0,
        .out_color_format = (lcd_color_format_t)0,
        .num_fbs = 1,
        .video_timing =
            {
                .h_size = 1920,
                .v_size = 1080,
                // From the NED init code spreadsheet
                .hsync_pulse_width = 44,
                .hsync_back_porch = 148,
                .hsync_front_porch = 88,
                .vsync_pulse_width = 5,
                .vsync_back_porch = 36,
                .vsync_front_porch = 5,
            },
        .flags =
            {
                .use_dma2d = true,
                .disable_lp = false,
            },
    };
    return dpi_config;
}

static inline seeya049_vendor_config_t
panel_seeya049_vendor_config(esp_lcd_dsi_bus_handle_t mipi_dsi_bus,
                             esp_lcd_dpi_panel_config_t *dpi_config) {
    seeya049_vendor_config_t vendor_config = {
        .init_cmds = seeya049_init_cmds_mipi,
        .init_cmds_size = sizeof(seeya049_init_cmds_mipi) / sizeof(seeya049_lcd_init_cmd_t),
        .refresh_rate = 60,
        .x_offset = 0,
        .y_offset = 0,
        .mipi_config =
            {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = dpi_config,
                .lane_num = 2,
            },
    };
    return vendor_config;
}

static inline esp_lcd_panel_dev_config_t panel_seeya049_panel_config(int reset_gpio_num,
                                                                     void *vendor_config) {
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = reset_gpio_num,
        // These don't have an effect on the test patterns
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 24,
        .flags =
            {
                .reset_active_high = false,
            },
        .vendor_config = vendor_config,
    };
    return panel_config;
}

// inline seeya049_vendor_config_t panel_seeya049_1920_1080_vendor_config(void) {
// seeya049_vendor_config_t vendor_config = {
//     .init_cmds = NULL,
//     .init_cmds_size = 0,
//     .mipi_config =
//         {
//             .dsi_bus = mipi_dsi_bus,
//             .dpi_config = &dpi_config,
//             .lane_num = 2,
//         },
// };

// const esp_lcd_panel_dev_config_t panel_config = {
//     .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
//     // These don't have an effect on the test patterns
//     .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
//     .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
//     .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
//     .flags =
//         {
//             .reset_active_high = false,
//         },
//     .vendor_config = &vendor_config,
// };

#endif // SOC_MIPI_DSI_SUPPORTED

/**
 * @brief Initialize the I2C bus for the panel
 *
 * @return
 *      - ESP_OK on success
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_i2c_init(int sda_gpio_num, int scl_gpio_num);

/**
 * @brief Deinitialize the I2C bus for the panel
 *
 * @return
 *      - ESP_OK on success
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_i2c_deinit(void);

/**
 * @brief Write a register to the panel using I2C
 *
 * @param[in] device_addr I2C device address
 * @param[in] reg_addr Register address
 * @param[in] data Data to write
 * @return
 */
esp_err_t panel_seeya049_i2c_write_reg(uint8_t device_addr, uint16_t reg_addr, uint16_t data);

/**
 * @brief Read a register from the panel using I2C
 *
 * @param[in] device_addr I2C device address
 * @param[in] reg_addr Register address
 */
esp_err_t panel_seeya049_i2c_read_reg(uint8_t device_addr, uint16_t reg_addr, uint16_t *data);

/**
 * @brief Send commands to the panel using I2C
 *
 * @param[in] device_addr I2C device address
 * @param[in] init_cmds Commands to send
 * @param[in] size Number of commands to send
 * @return
 *      - ESP_OK on success
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_send_cmds_i2c(uint8_t device_addr,
                                       const seeya049_lcd_init_cmd_t *init_cmds, uint16_t size);

/**
 * @brief Send initialization commands to the panel using I2C
 *
 * @param[in] device_addr I2C device address
 * @param[in] width Width of the display
 * @param[in] height Height of the display
 * @param[in] x_offset X offset of the display
 * @param[in] y_offset Y offset of the display
 * @return
 *      - ESP_OK on success
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_send_init_cmds_i2c(uint8_t device_addr, uint16_t width, uint16_t height,
                                            uint16_t x_offset, uint16_t y_offset);

/**
 * @brief Move the display area of the panel using I2C
 *
 * @param[in] device_addr I2C device address
 * @param[in] width Width of the display (needed to calculate the correct x_offset)
 * @param[in] height Height of the display (needed to calculate the correct y_offset)
 * @param[in] x_offset X offset of the display
 * @param[in] y_offset Y offset of the display
 */
esp_err_t panel_seeya049_move_display_area_i2c(uint8_t device_addr, uint16_t width, uint16_t height,
                                               int16_t x_offset, int16_t y_offset);

#ifdef __cplusplus
}
#endif
