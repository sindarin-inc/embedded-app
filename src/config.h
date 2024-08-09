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
// #define DISABLE_POWER_MANAGEMENT 1

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

// Uncomment the following lines to force the use of TTF
// #undef CONFIG_FONT_IBMF
// #undef CONFIG_FONT_TTF
// #define CONFIG_FONT_TTF 1
