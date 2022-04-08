// Configuration defines
// WARNING: Based on how this file is pre-included everywhere, we don't trigger
// an entirely new build when this file changes. To make sure changes here get
// picked up you need to clean and build everything. Or make sure any files that
// need stuff from here explicitly #include "config.h"

#ifndef REVISION
#define REVISION 7
#endif

#ifndef EINKET013TT1
#define EINKET013TT1 1
#endif

#define Hastings20x40_3

// Currently using the App (BLE Server in firmware) is incompatible with
// Turn Touch (BLE client in firmware). Enable one of these options or the
// other.
#ifndef USE_TURN_TOUCH_REMOTE
#define USE_TURN_TOUCH_REMOTE 1
#endif
#ifndef USE_BLE_SERVER_REMOTE
#define USE_BLE_SERVER_REMOTE 1
#endif
#ifndef USE_BLE_SERVER_SETTINGS
#define USE_BLE_SERVER_SETTINGS 1
#endif
#ifndef USE_BLE_SERVER_IMPROV
#define USE_BLE_SERVER_IMPROV 1
#endif
#define USE_BLE                                                                                    \
    (USE_TURN_TOUCH_REMOTE || USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS ||                  \
     USE_BLE_SERVER_IMPROV)

#ifndef DISABLE_WIFI
#define DISABLE_WIFI 0
#endif
// #define DISABLE_SPIRAM

#ifndef ENABLE_OTA
#define ENABLE_OTA 1
#define HOSTNAME "sindarin"
#endif

// Disable Improve BLE WiFi provisioning service
// #define DISABLE_IMPROV

#ifndef ENABLE_POCKET
#define ENABLE_POCKET 1
#endif

#ifdef WROVER_KIT
#define RED_LED 0
#define GREEN_LED 2
#else
#define RED_LED 22
#define GREEN_LED 25
#endif

#define BAT_VOLTAGE_RIGHT 37
#define BAT_VOLTAGE_LEFT 38
#define BATT_CHANNEL_RIGHT ADC1_CHANNEL_1
#define BATT_CHANNEL_LEFT ADC1_CHANNEL_2

#if defined(REVSION) && REVISION < 7
// On revisions prior to 7, the screen's power was tied to pin 26 along with the backlight, so
// define the power pin to turn on display and keep it on.
#define DISPLAY_POWER 26
#else // Revision 7 and above defaults
#ifndef LEDC_BACKLIGHT
#define LEDC_BACKLIGHT 1
#endif
#ifndef ULP_BACKLIGHT
#define ULP_BACKLIGHT 0
#endif
#define BACKLIGHT_RIGHT 26
#define BACKLIGHT_LEFT 9
#endif

#define DISPLAY_CS 5
#define DISPLAY_OEI 21
#ifndef DISPLAY_RST
#define DISPLAY_RST 32
#endif
#define DISPLAY_BUSY 33

#ifndef DISABLE_BUSY
#define DISABLE_BUSY 0
#endif

#define CPU_FREQ_MHZ_HIGH 240
#if !DISABLE_POWER_MANAGEMENT
#define CPU_FREQ_MHZ_LOW 40
#else
#define CPU_FREQ_MHZ_LOW 80
#endif

// HACK for Adafruit_GFX. Adafruit_GFX needs this value, but if we set it globally, then Arduino
// complains it's already defined.
#ifndef ARDUINO
#define ARDUINO 10185
#endif
