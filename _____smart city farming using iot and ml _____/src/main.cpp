#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include "npk_model_data.h"
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

// ----------- WiFi & Blynk ----------
#define BLYNK_TEMPLATE_ID "TMPL3I2SQOkiu"
#define BLYNK_TEMPLATE_NAME "project competition"
#define BLYNK_AUTH_TOKEN "TztuSfqLC7cVEwwsnIL1Z_qzm3NS6XJg"
char ssid[] = "pradnya";
char pass[] = "47474747";

// ----------- Sensors & Pins ----------
#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_PIN 34
#define LDR_PIN 35

// ----------- LCD Pins ----------
#define LCD_RS 18
#define LCD_EN 19
#define LCD_D4 21
#define LCD_D5 22
#define LCD_D6 23
#define LCD_D7 5
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ----------- Blynk Virtual Pins ----------
#define VP_TEMP V0
#define VP_HUM V1
#define VP_LDR V2
#define VP_SOIL V3
#define VP_NPK V5
#define VP_PH V6

// ----------- TFLite Setup ----------
namespace {
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  tflite::AllOpsResolver resolver;
  tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = nullptr;
  TfLiteTensor* input = nullptr;
  TfLiteTensor* output = nullptr;

  constexpr int tensor_arena_size = 4 * 1024;
  uint8_t tensor_arena[tensor_arena_size];
}

// ----------- States ----------
unsigned long lastLcdSwitch = 0;
int screenState = 0;  // 0 = NPK+PH, 1 = Temp+Hum+Soil+Light

void setupTFLite() {
  error_reporter = &micro_error_reporter;
  model = tflite::GetModel(npk_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1);
  }
  interpreter = new tflite::MicroInterpreter(model, resolver, tensor_arena, tensor_arena_size, error_reporter);
  interpreter->AllocateTensors();
  input = interpreter->input(0);
  output = interpreter->output(0);
}

BLYNK_CONNECTED() {
  // No need to sync virtual pins related to pump as it's removed
}

void setup() {
  Serial.begin(115200);

  lcd.begin(16, 2);
  lcd.print("Smart Farming");
  delay(2000);
  lcd.clear();

  dht.begin();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  setupTFLite();
}

void loop() {
  Blynk.run();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int light = analogRead(LDR_PIN);
  int soil = analogRead(SOIL_PIN);
  float soilPercent = map(soil, 0, 4095, 100, 0);

  float n_temp = temp / 100.0;
  float n_hum = hum / 100.0;
  float n_light = light / 4095.0;
  float n_soil = soil / 4095.0;

  input->data.f[0] = n_temp;
  input->data.f[1] = n_hum;
  input->data.f[2] = n_light;
  input->data.f[3] = n_soil;

  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("TFLite inference failed!");
    return;
  }

  float predicted_n = output->data.f[0] * 100;
  float predicted_p = output->data.f[1] * 100;
  float predicted_k = output->data.f[2] * 100;
  float predicted_ph = output->data.f[3] * 14;

  Blynk.virtualWrite(VP_TEMP, temp);
  Blynk.virtualWrite(VP_HUM, hum);
  Blynk.virtualWrite(VP_LDR, light);
  Blynk.virtualWrite(VP_SOIL, soilPercent);
  Blynk.virtualWrite(VP_NPK, String("N: ") + predicted_n + " P: " + predicted_p + " K: " + predicted_k);
  Blynk.virtualWrite(VP_PH, predicted_ph);

  // LCD rotation logic
  if (millis() - lastLcdSwitch > 5000) {
    screenState = (screenState + 1) % 2;
    lastLcdSwitch = millis();
    lcd.clear();
  }

  if (screenState == 0) {
    lcd.setCursor(0, 0);
    lcd.print("N:");
    lcd.print(predicted_n, 0);
    lcd.print(" P:");
    lcd.print(predicted_p, 0);
    lcd.setCursor(0, 1);
    lcd.print("K:");
    lcd.print(predicted_k, 0);
    lcd.print(" pH:");
    lcd.print(predicted_ph, 1);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temp, 0);
    lcd.print(" H:");
    lcd.print(hum, 0);
    lcd.setCursor(0, 1);
    lcd.print("S:");
    lcd.print(soilPercent, 0);
    lcd.print(" L:");
    lcd.print(light);
  }

  delay(1000);
}
