#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int xPin = A0;
const int yPin = A1;
const int zPin = A2;

// Threshold and calibration variables
float threshold = 0.5;        // Adjust this based on testing
float xRest = 0.20;          // Your measured rest values
float yRest = 0.21;
float zRest = 1.35;

// Step counting variables
int stepCount = 0;
bool stepDetected = false;
unsigned long lastStepTime = 0;
const unsigned long minStepInterval = 300; // Minimum 300ms between steps

// Constants for conversion
const float arduinoVref = 5.0;
const float adxlSupply = 3.3;
const float zeroG = adxlSupply / 2.0;
const float sensitivity = 0.33;

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  
  lcd.setCursor(0, 0);
  lcd.print("Step Counter");
  lcd.setCursor(0, 1);
  lcd.print("Calibrating...");
  delay(2000);
  
  // Quick calibration - measure rest values
  calibrateRest();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Steps: 0");
  lcd.setCursor(0, 1);
  lcd.print("Ready to walk!");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Read accelerometer values
  float x_g = readAxisG(xPin);
  float y_g = readAxisG(yPin);
  float z_g = readAxisG(zPin);
  
  // Calculate acceleration magnitude relative to rest position
  float magnitude = sqrt(pow(x_g - xRest, 2) + 
                        pow(y_g - yRest, 2) + 
                        pow(z_g - zRest, 2));
  
  // Step detection logic
  if (magnitude > threshold && !stepDetected && 
      (currentTime - lastStepTime > minStepInterval)) {
    stepCount++;
    stepDetected = true;
    lastStepTime = currentTime;
    
    // Brief feedback
    lcd.setCursor(0, 1);
    lcd.print("Step detected!  ");
    delay(100);
  }
  
  // Reset step detection when magnitude drops below threshold
  if (magnitude < (threshold * 0.7)) {
    stepDetected = false;
  }
  
  // Update LCD every 200ms
  static unsigned long lastDisplayUpdate = 0;
  if (currentTime - lastDisplayUpdate > 200) {
    updateDisplay(magnitude);
    lastDisplayUpdate = currentTime;
  }
  
  // Debug output to Serial
  Serial.print("Mag: "); Serial.print(magnitude, 3);
  Serial.print(" | Steps: "); Serial.println(stepCount);
  
  delay(50); // 20Hz sampling rate
}

float readAxisG(int pin) {
  int rawValue = analogRead(pin);
  float voltage = (rawValue * arduinoVref) / 1023.0;
  return (voltage - zeroG) / sensitivity;
}

void calibrateRest() {
  float xSum = 0, ySum = 0, zSum = 0;
  const int samples = 50;
  
  for (int i = 0; i < samples; i++) {
    xSum += readAxisG(xPin);
    ySum += readAxisG(yPin);
    zSum += readAxisG(zPin);
    delay(20);
  }
  
  xRest = xSum / samples;
  yRest = ySum / samples;
  zRest = zSum / samples;
  
  Serial.print("Calibrated rest values - X: "); Serial.print(xRest, 3);
  Serial.print(" Y: "); Serial.print(yRest, 3);
  Serial.print(" Z: "); Serial.println(zRest, 3);
}

void updateDisplay(float magnitude) {
  lcd.setCursor(0, 0);
  lcd.print("Steps: ");
  lcd.print(stepCount);
  lcd.print("        ");
  
  lcd.setCursor(0, 1);
  lcd.print("Mag: ");
  lcd.print(magnitude, 2);
  lcd.print("        ");
}
