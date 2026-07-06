#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int xPin = A0;
const int yPin = A1;
const int zPin = A2;

// Constants for 3.3V ADXL335 with 5V Arduino reference
const float arduinoVref = 5.0;        // Arduino's analog reference (5V)
const float adxlSupply = 3.3;         // ADXL335 supply voltage
const float zeroG = adxlSupply / 2.0;  // 1.65V for 3.3V supply
const float sensitivity = 0.33;       // 0.33V/g

void setup() {
  // No analogReference() call - use default 5V reference
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
}

void loop() {
  int xRaw = analogRead(xPin);
  int yRaw = analogRead(yPin);
  int zRaw = analogRead(zPin);

  // Convert to actual voltage (accounting for 5V reference)
  float xVolt = (xRaw * arduinoVref) / 1023.0;
  float yVolt = (yRaw * arduinoVref) / 1023.0;
  float zVolt = (zRaw * arduinoVref) / 1023.0;

  // Convert to acceleration (g)
  float x_g = (xVolt - zeroG) / sensitivity;
  float y_g = (yVolt - zeroG) / sensitivity;
  float z_g = (zVolt - zeroG) / sensitivity;

  lcd.setCursor(0, 0);
  lcd.print("X:");
  lcd.print(x_g, 2);
  lcd.print(" Y:");
  lcd.print(y_g, 2);

  lcd.setCursor(0, 1);
  lcd.print("Z:");
  lcd.print(z_g, 2);
  lcd.print("g        ");

  delay(500);

  // Add this to your loop() to see actual voltages
  Serial.print("X voltage: "); Serial.print(xVolt, 3); Serial.print("V, ");
  Serial.print("Y voltage: "); Serial.print(yVolt, 3); Serial.print("V, ");
  Serial.print("Z voltage: "); Serial.print(zVolt, 3); Serial.println("V");

}
