#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Pin Definitions ---
#define POT_PIN       34
#define DHT_PIN       15
#define DHT_TYPE      DHT22
#define GREEN_LED     12
#define RED_LED       14

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Wi-Fi & Adafruit IO Credentials ---
#define WLAN_SSID       "Wokwi-GUEST" 
#define WLAN_PASS       ""            
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "raj12" 
#define AIO_KEY         "aio_aoWg38lvT63BnR7e1ahm5WBTH67U" 

// --- Global State Variables ---
enum MonitoringMode { MODE_AUTO, MODE_MANUAL };
MonitoringMode currentMode = MODE_AUTO;
int currentSamplingRate = 5; 
int manualSliderRate = 30;    

float currentBPM = 75.0;
float currentTemp = 37.0;
bool isEmergency = false;

// --- FreeRTOS Queue Handles ---
QueueHandle_t samplingRateQueue;

// --- Setup WiFi and MQTT Clients ---
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// --- MQTT Feeds ---
Adafruit_MQTT_Publish pubBPM = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/heart-rate");
Adafruit_MQTT_Publish pubTemp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish pubMode = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/sampling-mode");

// Initialize Sensors
DHT dht(DHT_PIN, DHT_TYPE);

// --- Function Prototypes ---
void MQTT_connect();
void vSensorTask(void *pvParameters);
void vProcessingTask(void *pvParameters);
void vDisplayTask(void *pvParameters);
void vMQTTTask(void *pvParameters);

void setup() {
  Serial.begin(115200);
  delay(10);
  
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  analogSetAttenuation(ADC_11db); 
  dht.begin();

  // Initialize OLED Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("OLED allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.display();

  // Connect to Virtual WiFi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");

  // Create FreeRTOS Queue 
  samplingRateQueue = xQueueCreate(1, sizeof(int));

  // --- Create FreeRTOS Tasks ---
  xTaskCreatePinnedToCore(vSensorTask, "Sensor Task", 3072, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(vProcessingTask, "Processing Task", 2048, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(vDisplayTask, "Display Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(vMQTTTask, "MQTT Task", 8192, NULL, 1, NULL, 0); 
}

void loop() {
  // Managed completely by FreeRTOS scheduler tasks.
}

// ==========================================
//           FREERTOS TASK DEFINITIONS
// ==========================================

void vSensorTask(void *pvParameters) {
  int localSamplingRate = 5; 

  for (;;) {
    if (xQueueReceive(samplingRateQueue, &localSamplingRate, 0) == pdTRUE) {
      currentSamplingRate = localSamplingRate;
    }

    digitalWrite(GREEN_LED, HIGH);
    vTaskDelay(pdMS_TO_TICKS(100)); 
    digitalWrite(GREEN_LED, LOW);

    int potValue = analogRead(POT_PIN);
    currentBPM = map(potValue, 0, 4095, 50, 140);
    
    if(currentBPM < 50) currentBPM = 50; 

    float t = dht.readTemperature();
    if (!isnan(t)) {
      currentTemp = t;
    }

    Serial.printf("[Sensor] BPM: %.1f | Temp: %.1fC | Rate: %ds\n", currentBPM, currentTemp, currentSamplingRate);

    vTaskDelay(pdMS_TO_TICKS(currentSamplingRate * 1000));
  }
}

void vProcessingTask(void *pvParameters) {
  int targetRate = 5;
  
  for (;;) {
    if (currentBPM > 100 || currentBPM < 60 || currentTemp > 39.0 || currentTemp < 35.0) {
      isEmergency = true;
      digitalWrite(RED_LED, HIGH); 
    } else {
      isEmergency = false;
      digitalWrite(RED_LED, LOW);
    }

    if (currentMode == MODE_AUTO) {
      targetRate = isEmergency ? 5 : 60; 
    } else {
      targetRate = manualSliderRate;     
    }

    if (targetRate != currentSamplingRate) {
      xQueueOverwrite(samplingRateQueue, &targetRate);
    }

    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}

void vDisplayTask(void *pvParameters) {
  for (;;) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    display.setTextSize(1);
    display.setCursor(0, 0);
    if (currentMode == MODE_AUTO) {
      display.print("MODE: AUTO");
    } else {
      display.print("MODE: MANUAL");
    }
    
    display.setCursor(95, 0);
    display.print(currentSamplingRate);
    display.print("s");
    
    display.drawFastHLine(0, 11, 128, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print("BPM: ");
    display.print((int)currentBPM);
    
    display.setCursor(0, 38);
    display.print("TMP: ");
    display.print(currentTemp, 1);
    display.print("C");

    if (isEmergency) {
      display.fillRect(0, 54, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setTextSize(1);
      display.setCursor(18, 55);
      display.print("CRITICAL STATE");
    }

    display.display();
    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}

void vMQTTTask(void *pvParameters) {
  TickType_t lastPublishTime = xTaskGetTickCount();
  
  for (;;) {
    MQTT_connect();

    // Pulled out subscription buffer loops to bypass JSON token crashes entirely
    if ((xTaskGetTickCount() - lastPublishTime) >= pdMS_TO_TICKS(currentSamplingRate * 1000)) {
      if (mqtt.connected()) {
        Serial.println("[MQTT] Publishing fresh vitals to dashboard...");
        if (!pubBPM.publish(currentBPM)) Serial.println("BPM Publish Failed");
        if (!pubTemp.publish(currentTemp)) Serial.println("Temp Publish Failed");
        if (!pubMode.publish((currentMode == MODE_AUTO) ? "AUTO" : "MANUAL")) Serial.println("Mode Publish Failed");
        lastPublishTime = xTaskGetTickCount();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}

void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected()) return;

  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
       Serial.println(mqtt.connectErrorString(ret));
       mqtt.disconnect(); 
       vTaskDelay(pdMS_TO_TICKS(2000));
       retries--;
       if (retries == 0) {
         Serial.println("Connection failed temporarily.");
         return; 
       }
  }
  Serial.println("MQTT Connected!");
}
