#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int xPin = A0;
const int yPin = A1;
const int zPin = A2;
const int stPin = 8;  // ADD THIS - ST pin connected to D8

// Constants for 3.3V ADXL335 with 5V Arduino reference
const float arduinoVref = 5.0;        
const float adxlSupply = 3.3;         
const float zeroG = adxlSupply / 2.0;  
const float sensitivity = 0.33;       

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  
  pinMode(stPin, OUTPUT);     // ADD THIS - Configure ST pin as output
  digitalWrite(stPin, LOW);   // ADD THIS - Start with self-test off
}

void loop() {
  // Read baseline values
  int xRaw = analogRead(xPin);
  int yRaw = analogRead(yPin);
  int zRaw = analogRead(zPin);

  float xVolt = (xRaw * arduinoVref) / 1023.0;
  float yVolt = (yRaw * arduinoVref) / 1023.0;
  float zVolt = (zRaw * arduinoVref) / 1023.0;

  Serial.print("BASELINE - X: "); Serial.print(xVolt, 3); 
  Serial.print("V, Y: "); Serial.print(yVolt, 3); 
  Serial.print("V, Z: "); Serial.print(zVolt, 3); Serial.println("V");

  // ADD THIS BLOCK - Self-test
  digitalWrite(stPin, HIGH);  // Activate self-test
  delay(100);                 // Wait for self-test to take effect
  
  // Read self-test values
  int xRawST = analogRead(xPin);
  int yRawST = analogRead(yPin);
  int zRawST = analogRead(zPin);

  float xVoltST = (xRawST * arduinoVref) / 1023.0;
  float yVoltST = (yRawST * arduinoVref) / 1023.0;
  float zVoltST = (zRawST * arduinoVref) / 1023.0;

  digitalWrite(stPin, LOW);   // Deactivate self-test

  // Calculate and display changes
  float xDelta = xVoltST - xVolt;
  float yDelta = yVoltST - yVolt;
  float zDelta = zVoltST - zVolt;

  Serial.print("SELF-TEST - X: "); Serial.print(xVoltST, 3); 
  Serial.print("V, Y: "); Serial.print(yVoltST, 3); 
  Serial.print("V, Z: "); Serial.print(zVoltST, 3); Serial.println("V");
  
  Serial.print("CHANGES - dX: "); Serial.print(xDelta, 3); 
  Serial.print("V, dY: "); Serial.print(yDelta, 3); 
  Serial.print("V, dZ: "); Serial.print(zDelta, 3); Serial.println("V");
  Serial.println("---");

  // Convert to acceleration for LCD display
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

  delay(2000); // Wait 2 seconds between self-tests
}
