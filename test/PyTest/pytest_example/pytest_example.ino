#include <Servo.h>

// To do the USB disconnect and connect
// Pin connected to the base of the NPN transistor (or gate of the MOSFET) through a 1K ohm resistor
const int powerControlPin = 2;

// To use the servo for our purpose
Servo myservo;
int servoPin = 9;

void setup() {
  // Set the power control pin as an output
  pinMode(powerControlPin, OUTPUT);

  // Set the initial state of the USB power to off
  digitalWrite(powerControlPin, LOW);

  // Attach the servo to the servo pin
  myservo.attach(servoPin);

  // Start serial communication at 9600 bps
  Serial.begin(9600);

  myservo.write(160); //Base position
  // myservo.detach();
}

void loop() {
  // Check if there is any data available on the serial port
  if (Serial.available()) {
    char command_type = Serial.read();

    if (command_type == 'u') {
      if (Serial.available()) {
        char command = Serial.read();
        if (command == '1') {
          // Turn on the USB power
          digitalWrite(powerControlPin, HIGH);
        } else if (command == '0') {
          // Turn off the USB power
          digitalWrite(powerControlPin, LOW);
        }
      }
    } else if (command_type == 'p') {
      String input = Serial.readStringUntil('\n');
      if (input == "detach") {
        // Detach the servo
        myservo.detach();
      } else {
        int pos = input.toInt();
        if (pos >= 0 && pos <= 180) {
          myservo.write(pos);
        }
      }
    }
  }
}

