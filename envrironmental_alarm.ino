/**
 * ESP32-S3 Real-Time Environmental Alarm System
 *
 * Dual-core FreeRTOS project using an AM2320 sensor, HC-SR04 ultrasonic
 * sensor, 16x2 I2C LCD, three status LEDs, and a push button.
 */

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_AM2320.h>

// Pin map
#define I2C_SDA 4
#define I2C_SCL 5
#define LCD_ADDR 0x27
#define TRIG_PIN 16
#define ECHO_PIN 15
#define GREEN_LED 10
#define YELLOW_LED 11
#define RED_LED 13
#define BUTTON_PIN 12

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
Adafruit_AM2320 am2320 = Adafruit_AM2320();

enum SensorType {
  SENSOR_ENV,
  SENSOR_DISTANCE
};

enum SystemStatus {
  STATUS_NORMAL,
  STATUS_WARNING,
  STATUS_ALERT
};

enum MonitorMode {
  MODE_DISTANCE,
  MODE_TEMP,
  MODE_HUMIDITY,
  MODE_COMFORT,
  MODE_FULL
};

struct SensorMessage {
  SensorType type;
  float temperature;
  float humidity;
  float distance;
  bool valid;
};

struct DisplayMessage {
  float temperature;
  float humidity;
  float distance;
  bool envValid;
  bool distValid;
  SystemStatus status;
  MonitorMode mode;
};

QueueHandle_t sensorQueue;
QueueHandle_t displayQueue;
volatile MonitorMode monitorMode = MODE_DISTANCE;

const char* statusText(SystemStatus status) {
  if (status == STATUS_NORMAL) return "OK";
  if (status == STATUS_WARNING) return "WARN";
  return "ALERT";
}

const char* modeText(MonitorMode mode) {
  if (mode == MODE_DISTANCE) return "DIST";
  if (mode == MODE_TEMP) return "TEMP";
  if (mode == MODE_HUMIDITY) return "HUM";
  if (mode == MODE_COMFORT) return "COMFORT";
  return "FULL";
}

SystemStatus worseStatus(SystemStatus a, SystemStatus b) {
  if (a == STATUS_ALERT || b == STATUS_ALERT) return STATUS_ALERT;
  if (a == STATUS_WARNING || b == STATUS_WARNING) return STATUS_WARNING;
  return STATUS_NORMAL;
}

float readDistanceCmOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return NAN;

  float distance = duration / 58.0;
  if (distance < 2.0 || distance > 400.0) return NAN;
  return distance;
}

float readDistanceCmFiltered() {
  const int samples = 5;
  float sum = 0;
  int count = 0;

  for (int i = 0; i < samples; i++) {
    float distance = readDistanceCmOnce();
    if (!isnan(distance)) {
      sum += distance;
      count++;
    }
    delay(10);
  }

  if (count == 0) return NAN;
  return sum / count;
}

SystemStatus distanceStatus(float distance, bool valid) {
  if (!valid || isnan(distance)) return STATUS_ALERT;
  if (distance > 30.0) return STATUS_NORMAL;
  if (distance > 15.0) return STATUS_WARNING;
  return STATUS_ALERT;
}

SystemStatus tempStatus(float temperature, bool valid) {
  if (!valid || isnan(temperature)) return STATUS_ALERT;
  if (temperature >= 18.0 && temperature <= 28.0) return STATUS_NORMAL;
  if ((temperature >= 15.0 && temperature < 18.0) ||
      (temperature > 28.0 && temperature <= 32.0)) {
    return STATUS_WARNING;
  }
  return STATUS_ALERT;
}

SystemStatus humidityStatus(float humidity, bool valid) {
  if (!valid || isnan(humidity)) return STATUS_ALERT;
  if (humidity >= 30.0 && humidity <= 60.0) return STATUS_NORMAL;
  if ((humidity >= 20.0 && humidity < 30.0) ||
      (humidity > 60.0 && humidity <= 70.0)) {
    return STATUS_WARNING;
  }
  return STATUS_ALERT;
}

SystemStatus computeSystemStatus(
  MonitorMode mode,
  float temperature,
  float humidity,
  float distance,
  bool envValid,
  bool distValid
) {
  SystemStatus temperatureState = tempStatus(temperature, envValid);
  SystemStatus humidityState = humidityStatus(humidity, envValid);
  SystemStatus distanceState = distanceStatus(distance, distValid);

  if (mode == MODE_DISTANCE) return distanceState;
  if (mode == MODE_TEMP) return temperatureState;
  if (mode == MODE_HUMIDITY) return humidityState;
  if (mode == MODE_COMFORT) {
    return worseStatus(temperatureState, humidityState);
  }

  return worseStatus(
    worseStatus(temperatureState, humidityState),
    distanceState
  );
}

void setLEDs(SystemStatus status) {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);

  if (status == STATUS_NORMAL) {
    digitalWrite(GREEN_LED, HIGH);
  } else if (status == STATUS_WARNING) {
    digitalWrite(YELLOW_LED, HIGH);
  } else {
    digitalWrite(RED_LED, HIGH);
  }
}

void AM2320Task(void* parameter) {
  TickType_t lastWake = xTaskGetTickCount();

  while (true) {
    SensorMessage message;
    message.type = SENSOR_ENV;
    message.distance = NAN;

    float temperature = am2320.readTemperature();
    float humidity = am2320.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      message.valid = false;
      message.temperature = NAN;
      message.humidity = NAN;
      Serial.println("[AM2320Task] ERROR");
    } else {
      message.valid = true;
      message.temperature = temperature;
      message.humidity = humidity;

      Serial.print("[AM2320Task] T=");
      Serial.print(temperature);
      Serial.print(" C H=");
      Serial.print(humidity);
      Serial.println(" %");
    }

    xQueueSend(sensorQueue, &message, 0);
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
  }
}

void UltrasonicTask(void* parameter) {
  TickType_t lastWake = xTaskGetTickCount();

  while (true) {
    SensorMessage message;
    message.type = SENSOR_DISTANCE;
    message.temperature = NAN;
    message.humidity = NAN;

    float distance = readDistanceCmFiltered();

    if (isnan(distance)) {
      message.valid = false;
      message.distance = NAN;
      Serial.println("[UltrasonicTask] No echo");
    } else {
      message.valid = true;
      message.distance = distance;

      Serial.print("[UltrasonicTask] D=");
      Serial.print(distance);
      Serial.println(" cm");
    }

    xQueueSend(sensorQueue, &message, 0);
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(250));
  }
}

void ControlTask(void* parameter) {
  TickType_t lastWake = xTaskGetTickCount();

  float latestTemperature = NAN;
  float latestHumidity = NAN;
  float latestDistance = NAN;
  bool envValid = false;
  bool distValid = false;

  while (true) {
    SensorMessage message;

    while (xQueueReceive(sensorQueue, &message, 0) == pdTRUE) {
      if (message.type == SENSOR_ENV && message.valid) {
        envValid = true;
        latestTemperature = message.temperature;
        latestHumidity = message.humidity;
      }

      if (message.type == SENSOR_DISTANCE && message.valid) {
        distValid = true;
        latestDistance = message.distance;
      }
    }

    MonitorMode currentMode = monitorMode;
    SystemStatus status = computeSystemStatus(
      currentMode,
      latestTemperature,
      latestHumidity,
      latestDistance,
      envValid,
      distValid
    );

    setLEDs(status);

    DisplayMessage displayMessage;
    displayMessage.temperature = latestTemperature;
    displayMessage.humidity = latestHumidity;
    displayMessage.distance = latestDistance;
    displayMessage.envValid = envValid;
    displayMessage.distValid = distValid;
    displayMessage.status = status;
    displayMessage.mode = currentMode;

    xQueueOverwrite(displayQueue, &displayMessage);
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(20));
  }
}

void LCDTask(void* parameter) {
  TickType_t lastWake = xTaskGetTickCount();
  DisplayMessage message;
  MonitorMode lastMode = MODE_FULL;

  while (true) {
    if (xQueueReceive(displayQueue, &message, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (message.mode != lastMode) {
        lcd.clear();
        lastMode = message.mode;
      }

      char line1[17];
      snprintf(
        line1,
        sizeof(line1),
        "M:%-7s %-5s",
        modeText(message.mode),
        statusText(message.status)
      );

      lcd.setCursor(0, 0);
      lcd.print(line1);

      char line2[17];

      if (message.mode == MODE_DISTANCE) {
        if (message.distValid) {
          snprintf(line2, sizeof(line2), "D:%3.0fcm        ", message.distance);
        } else {
          snprintf(line2, sizeof(line2), "D:ERROR         ");
        }
      } else if (message.mode == MODE_TEMP) {
        if (message.envValid) {
          snprintf(line2, sizeof(line2), "T:%4.1fC        ", message.temperature);
        } else {
          snprintf(line2, sizeof(line2), "T:ERROR         ");
        }
      } else if (message.mode == MODE_HUMIDITY) {
        if (message.envValid) {
          snprintf(line2, sizeof(line2), "H:%2.0f%%          ", message.humidity);
        } else {
          snprintf(line2, sizeof(line2), "H:ERROR         ");
        }
      } else if (message.mode == MODE_COMFORT) {
        if (message.envValid) {
          snprintf(
            line2,
            sizeof(line2),
            "T:%2.0f H:%2.0f%%     ",
            message.temperature,
            message.humidity
          );
        } else {
          snprintf(line2, sizeof(line2), "ENV ERROR       ");
        }
      } else {
        if (message.envValid && message.distValid) {
          snprintf(
            line2,
            sizeof(line2),
            "T:%2.0f H:%2.0f D:%2.0f",
            message.temperature,
            message.humidity,
            message.distance
          );
        } else {
          snprintf(line2, sizeof(line2), "SENSOR ERROR    ");
        }
      }

      lcd.setCursor(0, 1);
      lcd.print(line2);
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(250));
  }
}

void ButtonTask(void* parameter) {
  bool lastReading = HIGH;
  bool stableState = HIGH;
  unsigned long lastChange = 0;
  const unsigned long debounceMs = 50;

  while (true) {
    bool reading = digitalRead(BUTTON_PIN);

    if (reading != lastReading) {
      lastChange = millis();
      lastReading = reading;
    }

    if ((millis() - lastChange) > debounceMs && reading != stableState) {
      stableState = reading;

      if (stableState == LOW) {
        int nextMode = (static_cast<int>(monitorMode) + 1) % 5;
        monitorMode = static_cast<MonitorMode>(nextMode);

        Serial.print("[ButtonTask] Mode changed to ");
        Serial.println(modeText(monitorMode));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== ESP32-S3 Environmental Alarm ===");

  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Env Alarm");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  if (!am2320.begin()) {
    Serial.println("AM2320 begin failed");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("AM2320 FAIL");
    delay(1000);
  } else {
    Serial.println("AM2320 begin OK");
  }

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  sensorQueue = xQueueCreate(10, sizeof(SensorMessage));
  displayQueue = xQueueCreate(1, sizeof(DisplayMessage));

  if (sensorQueue == nullptr || displayQueue == nullptr) {
    Serial.println("Queue creation failed");
    while (true) {
      delay(1000);
    }
  }

  digitalWrite(GREEN_LED, HIGH);
  delay(200);
  digitalWrite(GREEN_LED, LOW);

  digitalWrite(YELLOW_LED, HIGH);
  delay(200);
  digitalWrite(YELLOW_LED, LOW);

  digitalWrite(RED_LED, HIGH);
  delay(200);
  digitalWrite(RED_LED, LOW);

  xTaskCreatePinnedToCore(AM2320Task, "AM2320Task", 4096, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(UltrasonicTask, "UltrasonicTask", 4096, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(ControlTask, "ControlTask", 4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(LCDTask, "LCDTask", 4096, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(ButtonTask, "ButtonTask", 2048, nullptr, 1, nullptr, 1);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Running");
  lcd.setCursor(0, 1);
  lcd.print("FreeRTOS OK");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
