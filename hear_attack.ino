//devanesans.io@gmail.com
//User Name:hear_attack
//Pw: Projectiot@123


#include "health_model.h"
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <LiquidCrystal.h>

// -------------------- Arduino IoT Cloud --------------------
const char DEVICE_LOGIN_NAME[] = "2427e559-23e8-4b92-ab2f-55328d40047b";
const char SSID[] = "projectiot";          // Your WiFi SSID
const char PASS[] = "123456789aa";         // Your WiFi Password
const char DEVICE_KEY[] = "r8EjEz9wE8gBr8A3KEA6cbq#E";

String status;
int blood_Oxygen;
int heart_Rate;
String ai_label = "";

// -------------------- LCD --------------------
LiquidCrystal lcd(2, 15, 19, 18, 5, 4);

// -------------------- Constants --------------------
#define REPORTING_PERIOD_MS 1000
#define BUZZER_PIN 13

// -------------------- Pulse Oximeter --------------------
PulseOximeter pox;
uint32_t tsLastReport = 0;
void onBeatDetected() {}

// -------------------- WiFi Connection --------------------
WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

// -------------------- Tasks --------------------
TaskHandle_t sensorTaskHandle;
TaskHandle_t cloudTaskHandle;

// -------------------- Initialize Cloud Properties --------------------
void initProperties() {
  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);
  ArduinoCloud.addProperty(status, READWRITE, 5 * SECONDS, [](){});
  ArduinoCloud.addProperty(blood_Oxygen, READWRITE, 5 * SECONDS, [](){});
  ArduinoCloud.addProperty(heart_Rate, READWRITE, 5 * SECONDS, [](){});
}

// -------------------- Rule-Based AI --------------------
String simpleAI(int H, int S) {
  int score = 0;

  // HR points
  if(H < HR_NORMAL_MIN) score += 1;       // Bradycardia
  else if(H > HR_NORMAL_MAX) score += 2;  // Tachycardia

  // SpO2 points
  if(S < SPO2_MILD_MIN) score += 2;       // Severe Hypoxia
  else if(S < SPO2_NORMAL_MIN) score += 1;// Mild Hypoxia

  // Map score to labels
  if(score == 0) return AI_LABELS[0];       // Normal
  else if(score <= 2) return AI_LABELS[1];  // Mild Risk
  else if(score <= 3) return AI_LABELS[2];  // Moderate Risk
  else return AI_LABELS[3];                 // High Risk
}

// -------------------- Sensor Task --------------------
void sensorTask(void *pvParameters) {
  int H = 0, S = 0;
  for (;;) {
    pox.update();

    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
      S = pox.getSpO2();
      H = pox.getHeartRate();
      tsLastReport = millis();

      // Update cloud variables
      blood_Oxygen = S;
      heart_Rate = H;

      // Clear LCD before writing to avoid overlap
      lcd.clear();

      // --- LCD HR & SpO2 ---
      lcd.setCursor(0, 0);
      lcd.print("HR:");
      lcd.setCursor(3, 0);
      lcd.print(H);
      lcd.setCursor(7, 0);
      lcd.print("BO:");
      lcd.setCursor(10, 0);
      lcd.print(S);

      // --- Status & Buzzer ---
      if (S > SPO2_NORMAL_MIN) {
        status = "Normal";
        lcd.setCursor(0, 1);
        lcd.print("NR"); // Normal
        digitalWrite(BUZZER_PIN, LOW);

        // Show AI only if not KF
        ai_label = simpleAI(H, S);
        lcd.setCursor(3, 1);
        lcd.print("AI:");
        lcd.setCursor(6, 1);
        lcd.print(ai_label);

      } 
      else if (H == 0 && S == 0) {
        status = "Keep Finger";
        lcd.setCursor(0, 1);
        lcd.print("KF"); // Keep Finger
        digitalWrite(BUZZER_PIN, LOW);
        ai_label = ""; // No AI

      } 
      else if (S == 0) {
        status = "Keep Finger";
        lcd.setCursor(0, 1);
        lcd.print("KF"); // Keep Finger
        ai_label = ""; // No AI

      } 
      else {
        status = "Ab Normal";
        lcd.setCursor(0, 1);
        lcd.print("AN"); // Abnormal
        digitalWrite(BUZZER_PIN, HIGH);

        // Show AI only if not KF
        ai_label = simpleAI(H, S);
        lcd.setCursor(3, 1);
        lcd.print("AI:");
        lcd.setCursor(6, 1);
        lcd.print(ai_label);
      }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// -------------------- Cloud Task --------------------
void cloudTask(void *pvParameters) {
  for (;;) {
    ArduinoCloud.update();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print(" Patient Health ");
  lcd.setCursor(0, 1);
  lcd.print(" Monitoring IOT ");
  delay(2000);
  lcd.clear();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize pulse oximeter
  if (!pox.begin()) {
    Serial.println("Pulse oximeter init FAILED");
    for (;;);
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);

  // Initialize IoT Cloud
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  // Create tasks
  xTaskCreatePinnedToCore(sensorTask, "SensorTask", 10000, NULL, 1, &sensorTaskHandle, 0);
  xTaskCreatePinnedToCore(cloudTask, "CloudTask", 10000, NULL, 1, &cloudTaskHandle, 1);
}

// -------------------- Main Loop --------------------
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);  // Idle main loop
}