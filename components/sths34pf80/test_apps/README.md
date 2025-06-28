# STHS34PF80 Test Application

This test application demonstrates and verifies the functionality of the STHS34PF80 human presence and motion detection sensor driver.

## Features Tested

- I2C communication initialization
- WHO_AM_I register verification
- Register read/write operations
- Basic sensor configuration
- Temperature data reading (object and ambient)
- Presence and motion detection
- One-shot measurement mode
- Multi-byte read/write operations

## Building and Running

1. Configure the test parameters (I2C pins and device address):
   ```
   idf.py menuconfig
   ```
   Navigate to "STHS34PF80 Test Configuration" to set:
   - I2C port number
   - I2C SCL GPIO number
   - I2C SDA GPIO number
   - Device I2C address (0x5A or 0x5B)

2. Build the test application:
   ```
   idf.py build
   ```

3. Flash and run:
   ```
   idf.py flash monitor
   ```

4. Once running, you'll see a menu with available tests. Select a test to run it individually, or run all tests.

## Hardware Setup

Connect the STHS34PF80 sensor to your ESP32:
- VDD to 3.3V
- GND to GND
- SCL to configured SCL GPIO
- SDA to configured SDA GPIO
- ADDR_SEL to GND for address 0x5A, or to VDD for address 0x5B

## Test Descriptions

### Initialization Test
Verifies the sensor responds correctly and returns the expected WHO_AM_I value (0xD3).

### Register Read/Write Test
Tests basic register read and write operations by modifying and restoring the AVG_TRIM register.

### Basic Configuration Test
Configures the sensor with Block Data Update (BDU) enabled and 1Hz output data rate.

### Temperature Data Reading Test
Reads object and ambient temperature raw values from the sensor.

### Presence Detection Test
Monitors presence and motion values continuously for several seconds.

### One-Shot Mode Test
Tests single measurement acquisition using one-shot mode.

### Multi-byte Operations Test
Verifies multi-byte read and write operations using the embedded function registers.