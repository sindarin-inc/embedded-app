// Configuration defines

//#define EINK154in
#define EINKET013TT1
//#define OLED96in
//#define SHARPMEM

//#define PlanoConvex20x40
//#define PlanoConvex16x36
#define Hastings20x40_3

// Currently using the App (BLE Server in firmware) is incompatible with
// Turn Touch (BLE client in firmware). Enable one of these options or the
// other.
#define USE_TURN_TOUCH_REMOTE
#define USE_BLE_SERVER_REMOTE
#define USE_BLE_SERVER_SETTINGS
#define USE_BLE_SERVER_IMPROV
// #define USE_IR_REMOTE

#define IR_REMOTE_INPUT_PIN 12

// #define DISABLE_WIFI
// #define DISABLE_SPIRAM

#define ENABLE_OTA
#define HOSTNAME "sindarin"

// Disable Improve BLE WiFi provisioning service
// #define DISABLE_IMPROV

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
