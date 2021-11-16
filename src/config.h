// Configuration defines

#define EINK154in
//#define OLED96in
//#define SHARPMEM

//#define PlanoConvex20x40
//#define PlanoConvex16x36
#define Hastings20x40_3

// Currently using the App (BLE Server in firmware) is incompatible with
// Turn Touch (BLE client in firmware). Enable one of these options or the
// other.
// #define USE_TURN_TOUCH_REMOTE
#define USE_BLE_SERVER_REMOTE
#define USE_IR_REMOTE

#define IR_REMOTE_INPUT_PIN 12

// #define DISABLE_WIFI
// #define DISABLE_SPIRAM
