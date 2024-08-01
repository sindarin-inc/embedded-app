// Configuration defines
// WARNING: Based on how this file is pre-included everywhere, we don't trigger
// an entirely new build when this file changes. To make sure changes here get
// picked up you need to clean and build everything. Or make sure any files that
// need stuff from here explicitly #include "config.h"

#pragma once

#ifndef REVISION
#define REVISION 7
#endif

// Uncomment to disable deep sleep
// #define DISABLE_DEEP_SLEEP 1

#define Hastings20x40_3

// Uncomment to disable power management when debugging on the rev1 dev board
// XXXBCBC: Should move this to the build system
#define DISABLE_POWER_MANAGEMENT 1

// Currently using the App (BLE Server in firmware) is incompatible with
// Turn Touch (BLE client in firmware). Enable one of these options or the
// other.
#ifndef CONFIG_USE_TURN_TOUCH_REMOTE
#define CONFIG_USE_TURN_TOUCH_REMOTE 1
#endif
#ifndef CONFIG_USE_BLE_SERVER_REMOTE
#define CONFIG_USE_BLE_SERVER_REMOTE 1
#endif
#ifndef CONFIG_USE_BLE_SERVER_SETTINGS
#define CONFIG_USE_BLE_SERVER_SETTINGS 1
#endif
#ifndef CONFIG_USE_BLE_SERVER_IMPROV
#define CONFIG_USE_BLE_SERVER_IMPROV 1
#endif
#define USE_BLE                                                                                    \
    (CONFIG_USE_TURN_TOUCH_REMOTE || CONFIG_USE_BLE_SERVER_REMOTE ||                               \
     CONFIG_USE_BLE_SERVER_SETTINGS || CONFIG_USE_BLE_SERVER_IMPROV)

// #define DISABLE_SPIRAM

#define HOSTNAME "solreader"

// Disable Improve BLE WiFi provisioning service
// #define DISABLE_IMPROV

#ifdef CONFIG_LED_INVERT
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#define BAT_VOLTAGE_RIGHT 10
#define BAT_VOLTAGE_LEFT 4
#define BATT_CHANNEL_RIGHT ADC_CHANNEL_9
#define BATT_CHANNEL_LEFT ADC_CHANNEL_3
#define UPPER_DIVIDER 442
#define LOWER_DIVIDER 160
// Unused on ESP32-S3
#define DEFAULT_VREF 0

#if defined(REVSION) && REVISION < 7
// On revisions prior to 7, the screen's power was tied to pin 26 along with the backlight, so
// define the power pin to turn on display and keep it on.
#define DISPLAY_POWER 26
#else // Revision 7 and above defaults
#ifndef CONFIG_LEDC_BACKLIGHT
#define CONFIG_LEDC_BACKLIGHT 1
#endif
#define BACKLIGHT_RIGHT 2
#define BACKLIGHT_LEFT 17
#endif

// Sol3
#define DISPLAY_CS_LEFT 21
#define DISPLAY_CS_RIGHT 11
#define USE_SYS_ON 1
#ifndef DISPLAY_RST
#define DISPLAY_RST 9
#endif

#define DISPLAY_SPI SPI2_HOST
#define DISPLAY_MOSI 12
#define DISPLAY_MISO 14
#define DISPLAY_CLK 13

#define DISPLAY_WAVESHARE_6INHD_MOSI 35
#define DISPLAY_WAVESHARE_6INHD_MISO 37
#define DISPLAY_WAVESHARE_6INHD_CLK 34
#define DISPLAY_WAVESHARE_6INHD_BUSY 6
#define DISPLAY_WAVESHARE_6INHD_CS 3
#define DISPLAY_WAVESHARE_6INHD_RESET 5

#define DISPLAY_SHARP_27IN_MOSI 35
#define DISPLAY_SHARP_27IN_CLK 34
#define DISPLAY_SHARP_27IN_CS 3
#define DISPLAY_SHARP_27IN_DISP 6

#define DISPLAY_WAVESHARE_128ROUND_MOSI 11
#define DISPLAY_WAVESHARE_128ROUND_MISO 12
#define DISPLAY_WAVESHARE_128ROUND_CLK 10
#define DISPLAY_WAVESHARE_128ROUND_DC 8
#define DISPLAY_WAVESHARE_128ROUND_RST 14
#define DISPLAY_WAVESHARE_128ROUND_CS 9

#define CPU_FREQ_MHZ_HIGH 240
#define CPU_FREQ_MHZ_LOW 40

#if (CONFIG_USE_LTR329 || CONFIG_USE_MMC5603)
#define USE_I2C 1
#else
#define USE_I2C 0
#endif

#if USE_I2C
#define I2C_MASTER_SCL_IO 0
#define I2C_MASTER_SDA_IO 1
#define I2C_MASTER_FREQ_HZ 400000
#endif

// This is a wart -- need to figure out how to have the right
// one passed in from the build system.
#if CONFIG_DISPLAY_WAVESHARE_6INHD
#undef CONFIG_FONT_IBMF
#define CONFIG_FONT_TTF 1

// By default, we'll just use ONE BIT font rendering and display, but the below
// will allow us to test out 8Bit should we want to
// #define CONFIG_WAVESHARE_8BIT 1
// This allows us to not actually do anything on the display through the driver
// for testing headless
#define CONFIG_WAVESHARE_HEADLESS 1
#endif

// Uncomment the following lines to force the use of TTF
// #undef CONFIG_FONT_IBMF
// #define CONFIG_FONT_TTF 1

#if CONFIG_FONT_TTF
// ToDo: The following macro could eventually be removed as PSRAM seems to be the way to go.
// Must be removed in combination with the ones in file TTFFontData.cpp
#define FT_USE_PSRAM 1
#define SHOW_CACHE_STATS 1
#define ShOW_FREE_STACK_SPACE 1
#endif

// Set DISPLAY_PIXEL_RESOLUTION_IS_FIX to 0 for displays that allow changing their pixel
// resolution dynamically.
#define DISPLAY_PIXEL_RESOLUTION_IS_FIX 1
