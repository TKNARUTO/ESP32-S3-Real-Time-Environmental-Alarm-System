# ESP32-S3 Environmental Alarm System

This is a small real-time monitoring system built with an ESP32-S3. It reads temperature, humidity, and distance, displays the results on an LCD, and turns on an LED when an alarm condition is met.

A push button is used to switch between different alarm modes.

## What it does

The system supports five modes:

1. Distance alarm
2. Temperature alarm
3. Humidity alarm
4. Temperature and humidity alarm
5. All sensors enabled

The LCD shows the current sensor readings and the selected mode.

## Hardware

* ESP32-S3 development board
* AM2320 temperature and humidity sensor
* HC-SR04 ultrasonic sensor
* 16×2 I2C LCD
* Push button
* LED and resistor
* Breadboard and jumper wires

## Pin setup

### LCD

| Signal  | ESP32-S3 Pin |
| ------- | ------------ |
| SDA     | GPIO 4       |
| SCL     | GPIO 5       |
| Address | `0x27`       |

### AM2320

| Signal | ESP32-S3 Pin |
| ------ | ------------ |
| SDA    | GPIO 18      |
| SCL    | GPIO 17      |

The LCD and AM2320 originally shared the same I2C bus. This caused unstable AM2320 readings, so I moved the sensor to a second I2C bus.

The remaining pin assignments can be found near the top of the firmware file.

## Software

The program is written in C++ using the Arduino ESP32 framework and FreeRTOS.

Different tasks are used for:

* Reading temperature and humidity
* Measuring distance
* Checking the push button
* Updating the LCD
* Controlling the alarm LED

The tasks run at different intervals so a slow sensor read does not stop the rest of the system.

## Project structure

```text
esp32s3-realtime-environmental-alarm/
├── README.md
├── firmware/
│   └── environmental_alarm.ino
├── docs/
│   ├── architecture.md
│   └── wiring.md
├── images/
│   ├── assembled_system.jpg
│   └── lcd_display.jpg
└── report/
    └── project_summary.pdf
```

## Required libraries

The following Arduino libraries are used:

* `LiquidCrystal_I2C`
* `Adafruit AM2320`
* `Adafruit Unified Sensor`

FreeRTOS is included with the ESP32 Arduino package.

## Running the project

1. Install Arduino IDE.
2. Install the ESP32 board package.
3. Select `ESP32-S3 Dev Module`.
4. Install the required libraries.
5. Open `firmware/environmental_alarm.ino`.
6. Select the correct serial port.
7. Compile and upload the program.
8. Open Serial Monitor at `115200` baud.

Example output:

```text
AM2320 OK | Temp: 27.60 C | Humidity: 34.00 %
HC-SR04 Distance: 17.75 cm
```

## Problems I ran into

### AM2320 communication errors

The AM2320 sometimes returned an error when it shared the I2C bus with the LCD.

I fixed most of the issue by using separate I2C buses:

* LCD on GPIO 4 and GPIO 5
* AM2320 on GPIO 18 and GPIO 17

### HC-SR04 no-echo readings

The ultrasonic sensor sometimes returned no echo because of loose wiring or an invalid pulse measurement.

The program checks for this condition instead of using it as a real distance value.

### Button input

A single button press could be detected more than once because of switch bouncing. A short debounce delay was added so each press changes the mode only once.

## Possible improvements

* Use FreeRTOS queues instead of shared global variables
* Add mutex protection for the LCD
* Save alarm thresholds in flash memory
* Allow the user to change thresholds
* Add Wi-Fi monitoring
* Record sensor readings over time
* Move the circuit from a breadboard to a PCB

## Author

Rui Kong
Electrical and Computer Engineering
University of Washington
