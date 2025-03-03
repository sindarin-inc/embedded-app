#include <Adafruit_ADS1X15.h>        // For ADS1015/ADS1115
#include <Adafruit_PWMServoDriver.h> // For PCA9685
#include <Wire.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Two ADS1115 devices at addresses 0x48 and 0x49
Adafruit_ADS1115 ads1; // Left eye (channels 0-3)
Adafruit_ADS1115 ads2; // Right eye (channels 0-3)

// Define the number of LEDs and phototransistors per eye
#define NUM_LEDS_PER_EYE 4
#define NUM_SENSORS_PER_EYE 4
#define TOTAL_LEDS 8

// LED indices: 0-3 for left eye, 4-7 for right eye
// Sensor indices: 0-3 on ads1 for left eye, 0-3 on ads2 for right eye

// LED pairs (left eye LED, right eye LED)
int ledPairs[NUM_LEDS_PER_EYE][2] = {
    {0, 4}, // Pair 0: LED 0 (left eye) and LED 4 (right eye)
    {1, 5}, // Pair 1: LED 1 (left eye) and LED 5 (right eye)
    {2, 6}, // Pair 2: LED 2 (left eye) and LED 6 (right eye)
    {3, 7}  // Pair 3: LED 3 (left eye) and LED 7 (right eye)
};

#define LED_ON_VALUE 4095
#define LED_OFF_VALUE 0

// Faster I2C speed for quicker communication
#define I2C_SPEED 400000 // 400kHz

// Timing parameters (microseconds)
#define LED_STABILIZATION_DELAY_US 1000 // 1ms for LED to stabilize
#define CYCLE_DELAY_US 5000             // 5ms between LED switches

// Buffer for faster serial output
char outputBuffer[512]; // Increased buffer size for safety

// Counter for data rows (helps with debugging)
unsigned long dataRowCounter = 0;

void setup() {
    // Start I2C at higher speed
    Wire.begin();
    Wire.setClock(I2C_SPEED);

    Serial.begin(1000000); // Use 1Mbps baud rate for faster serial communication

    // Initialize PCA9685
    pwm.begin();
    pwm.setPWMFreq(1600); // Increase PWM frequency for faster switching

    // Start ADS1115s on addresses 0x48 (left eye) & 0x49 (right eye)
    ads1.begin(0x48);
    ads2.begin(0x49);

    // Configure for faster readings
    // Set highest data rate possible
    ads1.setDataRate(RATE_ADS1115_860SPS); // 860 samples per second
    ads2.setDataRate(RATE_ADS1115_860SPS);

    // Set appropriate gain for your sensor range
    ads1.setGain(GAIN_ONE); // Adjust based on your signal levels
    ads2.setGain(GAIN_ONE);

    // Turn all LEDs off initially
    for (int i = 0; i < TOTAL_LEDS; i++) {
        pwm.setPWM(i, 0, LED_OFF_VALUE);
    }

    // Wait for serial connection
    while (!Serial) {
        delay(10);
    }

    // Print CSV header with clear labels
    Serial.println(
        "row,timestamp_ms,active_led_left,active_led_right,left_sensor_0,left_sensor_1,"
        "left_sensor_2,left_sensor_3,right_sensor_0,right_sensor_1,right_sensor_2,right_sensor_3");

    // Print a separator line to make the header stand out
    Serial.println(
        "---,----------,-------------,--------------,------------,------------,"
        "------------,------------,-------------,-------------,-------------,-------------");

    Serial.println("Setup complete. Beginning high-speed data collection...");
    Serial.println("NOTE: Phototransistors remain active continuously while cycling through LEDs");
    delay(1000);
}

void loop() {
    // Arrays to store sensor readings
    int16_t leftSensorValues[NUM_SENSORS_PER_EYE];
    int16_t rightSensorValues[NUM_SENSORS_PER_EYE];
    unsigned long timestamp;

    // Cycle through each LED pair (one LED per eye at a time)
    for (int ledPairIndex = 0; ledPairIndex < NUM_LEDS_PER_EYE; ledPairIndex++) {
        // Turn all LEDs off
        for (int i = 0; i < TOTAL_LEDS; i++) {
            pwm.setPWM(i, 0, LED_OFF_VALUE);
        }

        // Get the current LED pair
        int leftLedIndex = ledPairs[ledPairIndex][0];
        int rightLedIndex = ledPairs[ledPairIndex][1];

        // Turn on current LED pair
        pwm.setPWM(leftLedIndex, 0, LED_ON_VALUE);
        pwm.setPWM(rightLedIndex, 0, LED_ON_VALUE);

        // Allow LEDs to stabilize
        delayMicroseconds(LED_STABILIZATION_DELAY_US);

        // Get timestamp before reading sensors
        timestamp = millis();

        // Read all phototransistors (optimized for speed)
        for (int i = 0; i < NUM_SENSORS_PER_EYE; i++) {
            leftSensorValues[i] = ads1.readADC_SingleEnded(i);
            rightSensorValues[i] = ads2.readADC_SingleEnded(i);
        }

        // Format data into buffer for faster serial output
        // Include row counter, timestamp, and active LED indices
        // int pos = sprintf(outputBuffer, "%lu,%lu,%d,%d,", dataRowCounter++, timestamp,
        // leftLedIndex,
        //                   rightLedIndex);
        int pos = sprintf(outputBuffer, "");

        // Left eye sensor values
        for (int i = 0; i < NUM_SENSORS_PER_EYE; i++) {
            pos += sprintf(&outputBuffer[pos], "%d,", leftSensorValues[i]);
        }

        // Right eye sensor values
        for (int i = 0; i < NUM_SENSORS_PER_EYE - 1; i++) {
            pos += sprintf(&outputBuffer[pos], "%d,", rightSensorValues[i]);
        }
        pos += sprintf(&outputBuffer[pos], "%d", rightSensorValues[NUM_SENSORS_PER_EYE - 1]);

        // Send the entire buffer at once
        Serial.println(outputBuffer);
    }

    // Minimal delay between complete cycles
    delayMicroseconds(CYCLE_DELAY_US);
}
