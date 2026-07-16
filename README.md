# ESP32-S3 Real-Time Environmental Alarm System

This project is a real-time monitoring and alarm system built with an ESP32-S3.

It measures temperature, humidity, and object distance. The current readings and system status are shown on a 16x2 LCD. A push button switches between five monitoring modes, while green, yellow, and red LEDs indicate the current alarm level.

## Features

- Temperature and humidity measurement using an AM2320
- Distance measurement using an HC-SR04
- Five selectable monitoring modes
- Green, yellow, and red status LEDs
- 16x2 LCD output
- Push-button mode selection
- FreeRTOS tasks pinned across both ESP32-S3 cores
- Queue-based communication between sensor, control, and display tasks
- Invalid sensor reading and no-echo detection

## Monitoring Modes

The push button cycles through five modes:

1. Distance
2. Temperature
3. Humidity
4. Temperature and humidity
5. Full monitoring

The selected mode determines which sensor readings are used to calculate the current system status.

## System Status

The system uses three status levels:

| Status | LED | Meaning |
|---|---|---|
| Normal | Green | Sensor values are inside the normal range |
| Warning | Yellow | One or more values are near the configured limit |
| Alert | Red | A value is outside the allowed range or a sensor reading is invalid |

## Hardware

- ESP32-S3 development board
- AM2320 temperature and humidity sensor
- HC-SR04 ultrasonic distance sensor
- 16x2 I2C LCD
- Push button
- Green LED
- Yellow LED
- Red LED
- Current-limiting resistors
- Breadboard and jumper wires

## Pin Configuration

| Component | Signal | ESP32-S3 Pin |
|---|---|---|
| LCD and AM2320 | SDA | GPIO 4 |
| LCD and AM2320 | SCL | GPIO 5 |
| HC-SR04 | Trigger | GPIO 16 |
| HC-SR04 | Echo | GPIO 15 |
| Green LED | Output | GPIO 10 |
| Yellow LED | Output | GPIO 11 |
| Red LED | Output | GPIO 13 |
| Push Button | Input | GPIO 12 |

The LCD uses I2C address `0x27`.

The LCD and AM2320 share the same I2C bus on GPIO 4 and GPIO 5.

## Software Design

The firmware is written in C++ using the Arduino ESP32 framework and FreeRTOS.

Five tasks are used:

### AM2320Task

Reads temperature and humidity once per second.

If either reading is invalid, the task sends an error state instead of treating the value as valid sensor data.

### UltrasonicTask

Measures distance using the HC-SR04 every 250 milliseconds.

Five samples are collected and averaged. Invalid readings and missing echo pulses are ignored.

### ControlTask

Receives sensor messages through a FreeRTOS queue.

It stores the latest valid readings, checks the selected monitoring mode, calculates the system status, and updates the LEDs.

### LCDTask

Receives the latest display data through a single-item queue.

It shows the selected mode, alarm level, and relevant sensor readings on the LCD.

### ButtonTask

Reads the push button and changes the monitoring mode.

A debounce delay is used so that one physical button press only changes the mode once.

## Task Distribution

The sensor tasks run on Core 0:

- `AM2320Task`
- `UltrasonicTask`

The control and interface tasks run on Core 1:

- `ControlTask`
- `LCDTask`
- `ButtonTask`

This keeps sensor reading separate from display and control work.

## Queue Communication

Two FreeRTOS queues are used:

```text
sensorQueue
