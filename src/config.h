// Configuration defines

//#define EINK154in
#ifndef EINKET013TT1
#define EINKET013TT1 1
#endif
//#define OLED96in
//#define SHARPMEM

//#define PlanoConvex20x40
//#define PlanoConvex16x36
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
// #define USE_IR_REMOTE

#define IR_REMOTE_INPUT_PIN 12

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

#define RED_LED 22
#define GREEN_LED 25

#define BAT_VOLTAGE_RIGHT 37
#define BAT_VOLTAGE_LEFT 38
#define BATT_CHANNEL_RIGHT ADC1_CHANNEL_1
#define BATT_CHANNEL_LEFT ADC1_CHANNEL_2

#define DISPLAY_POWER 26
#define DISPLAY_CS 5
#define DISPLAY_OEI 21
#define DISPLAY_RST 32
#define DISPLAY_BUSY 33
