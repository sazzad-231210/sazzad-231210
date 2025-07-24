#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp32.h>

// DS18B20 libraries
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4  // D2 on ESP32 = GPIO 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Sensor pins
#define INPUT_VOLTAGE_SENSE_PIN 34
#define INPUT_CURRENT_SENSE_PIN 35

// Relay pins (active LOW)
#define RELAY_LIGHT 14
#define RELAY_FAN   23

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Blynk credentials
char auth[] = "PGNBn5EWpVO2XXf1q9gpnEOGjChmrJ2u";
char ssid[] = "solar";
char pass[] = "solarpoweriot";

// Measurement variables
float voltage1 = 0.0;
float amp = 0.0;
float power = 0.0;
float energy = 0.0;
float temperatureC = 0.0;
float BaseVol = 2.58; // Placeholder, auto-calibrated at startup

unsigned long last_time = 0;

// Function to auto-detect base voltage (when no current is flowing)
float readBaseVoltage(int pin) {
  float sum = 0;
  for (int i = 0; i < 500; i++) {
    sum += analogRead(pin);
    delay(3);
  }
  return sum / 500.0 * (3.3 / 4095.0);
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  digitalWrite(RELAY_LIGHT, HIGH);  // OFF state
  digitalWrite(RELAY_FAN, HIGH);

  pinMode(INPUT_VOLTAGE_SENSE_PIN, INPUT);
  pinMode(INPUT_CURRENT_SENSE_PIN, INPUT);

  Wire.begin();
  lcd.begin();
  lcd.backlight();

  sensors.begin();  // Start DS18B20

  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);

  // Auto-calibrate BaseVol (only works correctly if no current is flowing)
  BaseVol = readBaseVoltage(INPUT_CURRENT_SENSE_PIN);
  Serial.print("Auto-calibrated BaseVol: ");
  Serial.print(BaseVol, 3);
  Serial.println(" V");
}

// ========== BLYNK RELAY CONTROL ==========
BLYNK_WRITE(V3) {
  int state = param.asInt();
  digitalWrite(RELAY_LIGHT, state ? LOW : HIGH);
}

BLYNK_WRITE(V4) {
  int state = param.asInt();
  digitalWrite(RELAY_FAN, state ? LOW : HIGH);
}

void loop() {
  Blynk.run();

  // Voltage measurement (adjust based on your voltage divider)
  voltage1 = (((float)analogRead(INPUT_VOLTAGE_SENSE_PIN) / 4096) * 18.5 * 28205 / 27000);

  // Current measurement for ACS712-20A (Sensitivity = 0.1 V/A)
  float ACSValue = 0.0, Samples = 0.0, AvgACS = 0.0;
  for (int x = 0; x < 500; x++) {
    ACSValue = analogRead(INPUT_CURRENT_SENSE_PIN);
    Samples += ACSValue;
    delay(3);
  }
  AvgACS = Samples / 500.0;
  amp = ((((AvgACS) * (3.3 / 4095.0)) - BaseVol ) / 0.066); // 0.1 V/A for 20A
if (abs(amp) < 0.1) amp = 0.0; // Noise filter

  power = amp * voltage1;

  unsigned long current_time = millis();
  energy += power * ((current_time - last_time) / 3600000.0); // Energy in Wh
  last_time = current_time;

  // Read DS18B20 Temperature
  sensors.requestTemperatures();
  temperatureC = sensors.getTempCByIndex(0); // First sensor on bus

  // LCD Display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("V:");
  lcd.print(voltage1, 1);
   lcd.print("V");
  lcd.print(" I:");
  lcd.print(amp, 2);
   lcd.print("A");

  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temperatureC, 1);
  lcd.print("C P:");
  lcd.print(power, 1);

  // Blynk virtual pin updates
  Blynk.virtualWrite(V0, voltage1);
  Blynk.virtualWrite(V1, amp);
  Blynk.virtualWrite(V2, power);
  Blynk.virtualWrite(V5, energy);
  Blynk.virtualWrite(V6, temperatureC);

  delay(1000);
}
