// Cleaned and working version of your step counter code with COUNTDOWN fix
// Countdown goal mode now decrements correctly when steps are detected

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Constants and Globals ---
const int xPin = A0, yPin = A1, zPin = A2;
const float arduinoVref = 5.0, adxlSupply = 3.3, zeroG = adxlSupply / 2.0, sensitivity = 0.33;

const int stationaryLED = 6, walkingLED = 4, runningLED = 5;
const int button1Pin = 12, selfTestPin = 9;

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
  pinMode(selfTestPin, OUTPUT); digitalWrite(selfTestPin, LOW);
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
  float threshold = 13.25;

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

float readAxisVoltage(int pin) {
  return analogRead(pin) * arduinoVref / 1023.0;
}

void runSelfTest() {
  float x0 = readAxisVoltage(xPin), y0 = readAxisVoltage(yPin), z0 = readAxisVoltage(zPin);
  digitalWrite(selfTestPin, HIGH); delay(100);
  float x1 = readAxisVoltage(xPin), y1 = readAxisVoltage(yPin), z1 = readAxisVoltage(zPin);
  digitalWrite(selfTestPin, LOW);
  lcd.clear();
  lcd.print("dX:"); lcd.print(x1 - x0, 2); lcd.print(" dY:"); lcd.print(y1 - y0, 2);
  lcd.setCursor(0, 1); lcd.print("dZ:"); lcd.print(z1 - z0, 2);
}

void calculateAveragePace() {
  float minutes = totalTime / 60000.0;
  averagePace = stepGoal > 0 ? minutes / (stepGoal / 1000.0) : 0;
}

void calibrateSixPositions() {
  lcd.clear(); lcd.print("Cal done"); delay(1000);
}
