#include <Adafruit_ADS1X15.h>        // For ADS1015/ADS1115
#include <Adafruit_PWMServoDriver.h> // For PCA9685
#include <Wire.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Two ADS1115 devices at addresses 0x48 and 0x49
Adafruit_ADS1115 ads1;
Adafruit_ADS1115 ads2;

#define NUM_IR_LEDS 8
#define NUM_PAIRS 4

// Pairs: (LED0+4), (LED1+5), (LED2+6), (LED3+7)
int ledPairs[NUM_PAIRS][2] = {{0, 4}, {1, 5}, {2, 6}, {3, 7}};

#define LED_ON_VALUE 4095
#define LED_OFF_VALUE 0

void setup() {
    Wire.begin();
    Serial.begin(115200);

    // Initialize PCA9685
    pwm.begin();
    pwm.setPWMFreq(1000);

    // Start ADS1115s on addresses 0x48 & 0x49
    ads1.begin(0x48);
    ads2.begin(0x49);

    // Optionally configure gain, data rate, etc:
    // ads1.setGain(GAIN_ONE);
    // ads2.setGain(GAIN_ONE);
    // ads1.setDataRate(RATE_ADS1115_128SPS);
    // ads2.setDataRate(RATE_ADS1115_128SPS);

    Serial.println("Setup complete. Beginning test...");
}

void loop() {
    // Cycle through each of the 4 LED pairs
    for (int pairIndex = 0; pairIndex < NUM_PAIRS; pairIndex++) {

        // Turn ALL IR LEDs off first, so only the pair we care about is on
        for (int c = 0; c < NUM_IR_LEDS; c++) {
            pwm.setPWM(c, 0, LED_OFF_VALUE);
        }

        // Turn on the two LEDs in this pair
        int ledA = ledPairs[pairIndex][0];
        int ledB = ledPairs[pairIndex][1];
        pwm.setPWM(ledA, 0, LED_ON_VALUE);
        pwm.setPWM(ledB, 0, LED_ON_VALUE);

        // Measure continuously for 2 seconds
        unsigned long startTime = millis();
        while (millis() - startTime < 2000) {
            // Read the 8 phototransistors (4 from ads1, 4 from ads2)
            int16_t adcValues[8];
            for (int i = 0; i < 4; i++) {
                adcValues[i] = ads1.readADC_SingleEnded(i);
                adcValues[i + 4] = ads2.readADC_SingleEnded(i);
            }

            // Print them space-separated on one line for Arduino's Serial Plotter
            for (int i = 0; i < 8; i++) {
                Serial.print(adcValues[i]);
                if (i < 7) Serial.print(' ');
            }
            Serial.println();

            // Small delay to control how fast you sample/plot
            delay(50); // ~20 samples per second
        }

        // After 2 seconds, turn this pair off and move on to the next
    }

    // (Optional) a pause at the end of the full cycle
    // delay(1000);
}
