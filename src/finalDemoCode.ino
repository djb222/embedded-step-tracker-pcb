// Cleaned and working version of your step counter code with COUNTDOWN fix
// Countdown goal mode now decrements correctly when steps are detected

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Constants and Globals ---
const int xPin = A0, yPin = A1, zPin = A2;
const float arduinoVref = 5.0, adxlSupply = 3.3, zeroG = adxlSupply / 2.0, sensitivity = 0.33;

const int stationaryLED = 6, walkingLED = 4, runningLED = 5;
const int button1Pin = 12, stPin = 8;

enum Mode { MENU, NORMAL, STEP_GOAL, CALIBRATION, SELF_TEST, COUNTDOWN, RESULTS };
Mode currentMode = MENU;

const int menuItems = 4;
const char* menuLabels[menuItems] = {"Step Counter", "Step Goal", "Calibration", "Self-Test"};
int menuIndex = 0;

int stepGoalOptions[3] = {500, 1000, 2000};
int selectedStepGoalIndex = 0;
int stepGoal = 0, stepsRemaining = 0, stepCount = 0;
bool inCountdown = false;

enum PaceState { STATIONARY, WALKING, RUNNING };
PaceState currentPace = STATIONARY;

bool stepDetected = false;
const unsigned long minStepInterval = 300, paceUpdateInterval = 2000, LONG_PRESS_TIME = 1000;
unsigned long lastStepTime = 0, buttonPressStart = 0, lastGoalSelectTime = 0;
unsigned long countdownStartTime = 0, totalTime = 0;
float averagePace = 0;

// --- Function Declarations ---
void handleMenuButton(unsigned long);
void selectMenuItem();
void updateMenuDisplay();
void handleButtons(unsigned long);
void detectSteps(unsigned long);
void updatePaceFromStepRate(unsigned long);
void updateDisplay();
void displayNormalMode();
void displaySetupGoalMenu();
void displayCountdownMode();
void displayResultsMode();
void updateLEDs();
void resetSystem();
void runSelfTest();
void calibrateSixPositions();
float readAxisVoltage(int pin);
float readAxisG(int pin);
void calculateAveragePace();

// --- Setup ---
void setup() {
  pinMode(xPin, INPUT); pinMode(yPin, INPUT); pinMode(zPin, INPUT);
  pinMode(button1Pin, INPUT);
  pinMode(stationaryLED, OUTPUT); pinMode(walkingLED, OUTPUT); pinMode(runningLED, OUTPUT);
  pinMode(stPin, OUTPUT); digitalWrite(stPin, LOW);
  lcd.init(); lcd.backlight(); Serial.begin(9600);
  updateMenuDisplay();
}

// --- Main Loop ---
void loop() {
  unsigned long now = millis();
  static unsigned long lastLCDUpdate = 0;
  if (currentMode == MENU) { handleMenuButton(now); return; }
  handleButtons(now);
  if (currentMode == NORMAL || currentMode == STEP_GOAL || currentMode == COUNTDOWN) {
    detectSteps(now);
    updatePaceFromStepRate(now);
  }
  if (now - lastLCDUpdate > 200) {
    updateDisplay();
    updateLEDs();
    lastLCDUpdate = now;
  }
}

// --- Menu Button Handling ---
void handleMenuButton(unsigned long t) {
  static bool active = false;
  if (digitalRead(button1Pin) == HIGH) {
    if (!active) { buttonPressStart = t; active = true; }
    if (t - buttonPressStart > LONG_PRESS_TIME) {
      selectMenuItem(); active = false;
    }
  } else if (active) {
    menuIndex = (menuIndex + 1) % menuItems;
    updateMenuDisplay(); active = false;
  }
}

void selectMenuItem() {
  lcd.clear(); stepCount = 0;
  switch (menuIndex) {
    case 0: currentMode = NORMAL; break;
    case 1:
      currentMode = STEP_GOAL;
      selectedStepGoalIndex = 0;
      stepGoal = stepGoalOptions[selectedStepGoalIndex];
      lcd.print("Goal: "); lcd.print(stepGoal); lcd.print(" steps");
      delay(1000); stepsRemaining = stepGoal; break;
    case 2: currentMode = CALIBRATION; lcd.print("Calibrating..."); calibrateSixPositions(); delay(1500); resetSystem(); break;
    case 3: currentMode = SELF_TEST; lcd.print("Self-Test..."); delay(500); runSelfTest(); delay(3000); resetSystem(); break;
  }
}

void updateMenuDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Select Mode:");
  lcd.setCursor(0, 1); lcd.print(menuLabels[menuIndex]);
}

// --- Button Logic (short vs long press) ---
void handleButtons(unsigned long t) {
  static bool active = false;
  if (digitalRead(button1Pin) == HIGH) {
    if (!active) { buttonPressStart = t; active = true; }
    if (currentMode == STEP_GOAL && t - buttonPressStart > LONG_PRESS_TIME) {
      currentMode = COUNTDOWN; stepCount = 0; stepsRemaining = stepGoal;
      countdownStartTime = millis(); inCountdown = true; // ✅ FIXED: Set flag true
      lcd.clear(); active = false;
    }
  } else if (active) {
    if (currentMode == STEP_GOAL && t - buttonPressStart < LONG_PRESS_TIME) {
      selectedStepGoalIndex = (selectedStepGoalIndex + 1) % 3;
      stepGoal = stepGoalOptions[selectedStepGoalIndex];
      lcd.clear(); lcd.print("Step Goal:"); lcd.setCursor(0, 1);
      lcd.print(stepGoal); lcd.print(" steps");
      lastGoalSelectTime = t;
    }
    active = false;
  }
}

// --- Step Detection ---
void detectSteps(unsigned long t) {
  float x = readAxisG(xPin), y = readAxisG(yPin), z = readAxisG(zPin);
  float mag = sqrt(x * x + y * y + z * z);
  float threshold = 13.3;

  if (mag > threshold && !stepDetected && (t - lastStepTime > minStepInterval)) {
    if (currentMode == NORMAL) stepCount++;
    if (currentMode == COUNTDOWN && inCountdown && stepsRemaining > 0) {
      stepsRemaining--;
      Serial.print("Step detected, remaining: ");
      Serial.println(stepsRemaining);
      if (stepsRemaining == 0) {
        lcd.clear(); lcd.print("Goal reached!"); delay(2000);
        inCountdown = false;
        totalTime = millis() - countdownStartTime;
        calculateAveragePace();
        currentMode = RESULTS;
      }
    }
    stepDetected = true;
    lastStepTime = t;
  }
  if (mag < threshold) stepDetected = false;
}

// --- Pace State Update ---
void updatePaceFromStepRate(unsigned long t) {
  static unsigned long lastUpdate = 0;
  static int lastSteps = 0;
  if (t - lastUpdate > paceUpdateInterval) {
    int delta = stepCount - lastSteps;
    float rate = delta / (paceUpdateInterval / 1000.0);
    currentPace = rate < 0.5 ? STATIONARY : (rate < 2.0 ? WALKING : RUNNING);
    lastSteps = stepCount; lastUpdate = t;
  }
}

// --- LCD Display ---
void updateDisplay() {
  lcd.clear();
  switch (currentMode) {
    case NORMAL: displayNormalMode(); break;
    case STEP_GOAL: displaySetupGoalMenu(); break;
    case COUNTDOWN: displayCountdownMode(); break;
    case RESULTS: displayResultsMode(); break;
    default: break;
  }
}

void displayNormalMode() {
  lcd.setCursor(0, 0); lcd.print("Steps: "); lcd.print(stepCount);
  lcd.setCursor(0, 1); lcd.print("Pace: ");
  lcd.print(currentPace == STATIONARY ? "Still" : (currentPace == WALKING ? "Walk" : "Run "));
}

void displaySetupGoalMenu() {
  lcd.setCursor(0, 0); lcd.print("Set Step Goal:");
  lcd.setCursor(0, 1); lcd.print(stepGoal); lcd.print(" steps");
}

void displayCountdownMode() {
  lcd.setCursor(0, 0); lcd.print("Goal: "); lcd.print(stepGoal);
  lcd.setCursor(0, 1); lcd.print("Left: "); lcd.print(stepsRemaining);
}

void displayResultsMode() {
  lcd.setCursor(0, 0); lcd.print("Time: ");
  lcd.print(totalTime / 60000); lcd.print("m ");
  lcd.print((totalTime / 1000) % 60); lcd.print("s");
  lcd.setCursor(0, 1); lcd.print("Pace: "); lcd.print(averagePace, 1); lcd.print(" min/k");
}

// --- Utility Functions ---
void updateLEDs() {
  digitalWrite(stationaryLED, currentPace == STATIONARY);
  digitalWrite(walkingLED, currentPace == WALKING);
  digitalWrite(runningLED, currentPace == RUNNING);
}

void resetSystem() {
  stepCount = 0;
  currentMode = MENU;
  menuIndex = 0;
  updateMenuDisplay();
}

float readAxisG(int pin) {
  int raw = analogRead(pin);
  float voltage = raw * arduinoVref / 1023.0;
  return (voltage - zeroG) / sensitivity;
}

//float readAxisVoltage(int pin) {
//  return analogRead(pin) * arduinoVref / 1023.0;
//}

void runSelfTest() {

  float xBase = readAxisVoltage(xPin);

  float yBase = readAxisVoltage(yPin);

  float zBase = readAxisVoltage(zPin);
 
  digitalWrite(stPin, HIGH);

  delay(100);

  float xST = readAxisVoltage(xPin);

  float yST = readAxisVoltage(yPin);

  float zST = readAxisVoltage(zPin);
 
  digitalWrite(stPin, LOW);
 
  float xDelta = xST - xBase;

  float yDelta = yST - yBase;

  float zDelta = zST - zBase;
 
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("dX:");

  lcd.print(xDelta, 2);

  lcd.print(" dY:");

  lcd.print(yDelta, 2);
 
  lcd.setCursor(0, 1);

  lcd.print("dZ:");

  lcd.print(zDelta, 2);

  lcd.print("V");
 
  Serial.print("Self-Test dX: "); Serial.print(xDelta, 3);

  Serial.print(" dY: "); Serial.print(yDelta, 3);

  Serial.print(" dZ: "); Serial.println(zDelta, 3);

}
 

float readAxisVoltage(int pin) {
  int raw = analogRead(pin);
  return (raw * arduinoVref) / 1023.0;
}

void calculateAveragePace() {
  float minutes = totalTime / 60000.0;
  averagePace = stepGoal > 0 ? minutes / (stepGoal / 1000.0) : 0;
}

void calibrateSixPositions() {
  float readings[6][3]; // [position][axis: 0=X, 1=Y, 2=Z]
  const char* prompts[6] = {
    "+X UP", "-X UP", "+Y UP", "-Y UP", "+Z UP", "-Z UP"
  };
  int pos = 0;
 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("6-Pos Calibration");
  lcd.setCursor(0, 1);
  lcd.print("Press btn to start");
  while (digitalRead(button1Pin) == LOW); // Wait for release
  while (digitalRead(button1Pin) == HIGH); // Wait for press
 
  for (pos = 0; pos < 6; pos++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Orient: ");
    lcd.print(prompts[pos]);
    lcd.setCursor(0, 1);
    lcd.print("Press btn...");
    Serial.print("Place sensor in "); Serial.print(prompts[pos]); Serial.println(" orientation, then press button.");
 
    // Wait for user to press button
    while (digitalRead(button1Pin) == LOW);
    delay(200);
    while (digitalRead(button1Pin) == HIGH);
 
    // Take multiple samples and average
    float xSum = 0, ySum = 0, zSum = 0;
    const int samples = 50;
    for (int i = 0; i < samples; i++) {
      xSum += readAxisG(xPin);
      ySum += readAxisG(yPin);
      zSum += readAxisG(zPin);
      delay(10);
    }
    readings[pos][0] = xSum / samples;
    readings[pos][1] = ySum / samples;
    readings[pos][2] = zSum / samples;
 
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("X:");
    lcd.print(readings[pos][0], 2);
    lcd.print(" Y:");
    lcd.print(readings[pos][1], 2);
    lcd.setCursor(0, 1);
    lcd.print("Z:");
    lcd.print(readings[pos][2], 2);
    delay(1000);
  }
 
  // Calculate bias (zero-g offset) and sensitivity (scale) for each axis
  float xBias = (readings[0][0] + readings[1][0]) / 2.0;
  float yBias = (readings[2][1] + readings[3][1]) / 2.0;
  float zBias = (readings[4][2] + readings[5][2]) / 2.0;
  float xSens = (readings[0][0] - readings[1][0]) / 2.0;
  float ySens = (readings[2][1] - readings[3][1]) / 2.0;
  float zSens = (readings[4][2] - readings[5][2]) / 2.0;
 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bias X:");
  lcd.print(xBias, 2);
  lcd.setCursor(0, 1);
  lcd.print("Sns X:");
  lcd.print(xSens, 2);
  delay(5000);
 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bias Y:");
  lcd.print(yBias, 2);
  lcd.setCursor(0, 1);
  lcd.print("Sns Y:");
  lcd.print(ySens, 2);
  delay(5000);
 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bias Z:");
  lcd.print(zBias, 2);
  lcd.setCursor(0, 1);
  lcd.print("Sns Z:");
  lcd.print(zSens, 2);
  delay(5000);
 
  Serial.println("Calibration Complete:");
  Serial.print("X Bias: "); Serial.print(xBias, 4); Serial.print(" Sens: "); Serial.println(xSens, 4);
  Serial.print("Y Bias: "); Serial.print(yBias, 4); Serial.print(" Sens: "); Serial.println(ySens, 4);
  Serial.print("Z Bias: "); Serial.print(zBias, 4); Serial.print(" Sens: "); Serial.println(zSens, 4);
 
  // Store these values for use in your main code
  // (Optionally, write to EEPROM for persistent storage)
}
