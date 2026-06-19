# Adaptive-Rate RTOS Patient Monitor

An intelligent, resource-aware IoT Patient Monitoring System designed to track health metrics while maximizing hardware battery longevity. The system leverages **FreeRTOS** on an **ESP32** to dynamically adjust sensor sampling rates based on real-time patient stability profiles.

## 🚀 Key Features
- **Dynamic Sampling Architecture:** Alternates between stable baseline monitoring (60s intervals) and high-frequency emergency tracking (5s intervals) autonomously.
- **Multi-Tasking Kernel:** Implemented using FreeRTOS non-blocking delays (`vTaskDelay`) and inter-task communication queues (`xQueueOverwrite`).
- **IoT Cloud Integration:** Live telemetry processing over MQTT straight to an Adafruit IO dashboard.
- **Local Diagnostics:** Integrated $I^2C$ SSD1306 OLED display for immediate visual instrumentation alerts.

## 🛠️ Hardware & Components (Simulated in Wokwi)
- **ESP32 DevKit V1** (Core Processor running FreeRTOS)
- **DHT22 Sensor** (Simulated Patient Body Temperature)
- **Potentiometer** (Simulated Heart Rate Tracker / BPM)
- **SSD1306 OLED Display** (Local System Diagnostics UI)
- **LED Indicators** (System Heartbeat & Emergency Alarms)

## 💻 System Architecture (Task Configuration)
- **Sensor Task (Priority 2):** Samples analog/digital pins according to queue rhythms.
- **Processing Task (Priority 3):** Highest processing hierarchy; evaluates safety thresholds.
- **Display Task (Priority 1):** Renders operational data onto the local OLED.
- **MQTT Task (Priority 1):** Manages Wi-Fi handshakes and cloud telemetry streaming.
