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

#define USE_BLE                                                                                    \
    (CONFIG_USE_TURN_TOUCH_REMOTE || CONFIG_USE_BLE_SERVER_REMOTE ||                               \
     CONFIG_USE_BLE_SERVER_SETTINGS || CONFIG_USE_BLE_SERVER_IMPROV ||                             \
     CONFIG_USE_BLE_SERVER_PROXY)

// #define DISABLE_SPIRAM

#define HOSTNAME "solreader"

#ifdef CONFIG_LED_INVERT
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#if CONFIG_DISPLAY_EINKET013TT1 || CONFIG_DISPLAY_SHARP_27IN || CONFIG_DISPLAY_WAVESHARE_6INHD
#define CONFIG_DISPLAY_SPI SPI2_HOST
#endif

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
