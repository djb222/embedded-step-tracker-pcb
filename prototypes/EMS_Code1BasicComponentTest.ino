#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pin assignments
const int stationaryLED = 5;
const int walkingLED    = 6;
const int runningLED    = 7;
const int button1Pin    = 8;  // Single menu/mode button

// Modes and menu
enum Mode { MENU, NORMAL, STEP_GOAL, CALIBRATION, SELF_TEST, COUNTDOWN, RESULTS };
Mode currentMode = MENU;

const int menuItems = 4;
const char* menuLabels[menuItems] = {
  "Step Counter",
  "Step Goal",
  "Calibration",
  "Self-Test"
};
int menuIndex = 0;
const int stepGoalOptions[3] = {500, 1000, 2000};
int selectedStepGoalIndex = 0;
int stepGoal = 0;

// Step tracking and pace
enum PaceState { STATIONARY, WALKING, RUNNING };
PaceState currentPace = STATIONARY;
unsigned long lastStepTime = 0;
int stepCount = 0;
float simulatedAccel = 0;
const float stepThreshold = 0.7;
bool aboveThreshold = false;

// Timing and state control
unsigned long lastStateChange = 0;
const unsigned long minStateDuration = 5000;
const unsigned long maxStateDuration = 15000;
unsigned long stateDuration = 0;

// Button handling
const unsigned long LONG_PRESS_TIME = 1000;
unsigned long buttonPressStart = 0;
bool button1Active = false;

// Results
unsigned long countdownStartTime = 0;
unsigned long totalTime = 0;
float averagePace = 0;

// Function Prototypes
void handleMenuButton(unsigned long currentTime);
void selectMenuItem();
void updateMenuDisplay();
void handleButtons(unsigned long currentTime);
void handleLongPress();
void handleShortPress();
void generateSensorData(unsigned long currentTime);
void detectSteps(unsigned long currentTime);
void changeState();
void updateDisplay();
void displayNormalMode();
void displayStepGoalMode();
void displaySetupGoalMenu();
void displayCalibrationMode();
void displaySelfTestMode();
void displayCountdownMode();
void displayResultsMode();
void updateLEDs();
void resetSystem();
void calculateAveragePace();

void setup() {
  pinMode(stationaryLED, OUTPUT);
  pinMode(walkingLED, OUTPUT);
  pinMode(runningLED, OUTPUT);
  pinMode(button1Pin, INPUT);

  lcd.init();
  lcd.backlight();
  randomSeed(analogRead(A0));
  currentMode = MENU;
  menuIndex = 0;
  updateMenuDisplay();
}

void loop() {
  unsigned long currentTime = millis();
  static unsigned long lastLCDUpdate = 0;
  static unsigned long lastSimTime = 0;

  if (currentMode == MENU) {
    handleMenuButton(currentTime);
    return;
  }

  handleButtons(currentTime);

  if (currentMode == NORMAL || currentMode == STEP_GOAL || currentMode == COUNTDOWN) {
    if (currentTime - lastStateChange > stateDuration) {
      changeState();
    }
    if (currentTime - lastSimTime > 20) {
      lastSimTime = currentTime;
      generateSensorData(currentTime);
      detectSteps(currentTime);
    }
  }

  if (currentTime - lastLCDUpdate > 200) {
    updateDisplay();
    updateLEDs();
    lastLCDUpdate = currentTime;
  }
}

// --- MENU HANDLING ---
void handleMenuButton(unsigned long currentTime) {
  static bool buttonActive = false;
  static unsigned long buttonPressStart = 0;

  if (digitalRead(button1Pin) == HIGH) {
    if (!buttonActive) {
      buttonPressStart = currentTime;
      buttonActive = true;
    }
    if (currentTime - buttonPressStart > LONG_PRESS_TIME) {
      selectMenuItem();
      buttonActive = false;
    }
  } else {
    if (buttonActive) {
      // Short press: go to next menu item
      menuIndex = (menuIndex + 1) % menuItems;
      updateMenuDisplay();
      buttonActive = false;
    }
  }
}

void selectMenuItem() {
  lcd.clear();
  switch(menuIndex) {
    case 0:
      currentMode = NORMAL;
      stepCount = 0;
      break;
    case 1:
      currentMode = STEP_GOAL;
      selectedStepGoalIndex = 0;
      stepGoal = stepGoalOptions[selectedStepGoalIndex];
      stepCount = 0;
      lcd.clear();
      lcd.print("Goal: ");
      lcd.print(stepGoal);
      lcd.print(" steps");
      delay(1000);
      break;
    case 2:
      currentMode = CALIBRATION;
      lcd.clear();
      lcd.print("Calibration...");
      delay(1500);
      resetSystem();
      break;
    case 3:
      currentMode = SELF_TEST;
      lcd.clear();
      lcd.print("Self-Test...");
      delay(1500);
      resetSystem();
      break;
  }
}

void updateMenuDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select Mode:");
  lcd.setCursor(0, 1);
  lcd.print(menuLabels[menuIndex]);
}

// --- BUTTON HANDLING FOR OTHER MODES ---
void handleButtons(unsigned long currentTime) {
  static bool buttonActive = false;
  static unsigned long buttonPressStart = 0;

  if (digitalRead(button1Pin) == HIGH) {
    if (!buttonActive) {
      buttonPressStart = currentTime;
      buttonActive = true;
    }
    if (currentMode == STEP_GOAL && (currentTime - buttonPressStart > LONG_PRESS_TIME)) {
      // Long press to confirm step goal selection and start countdown
      currentMode = COUNTDOWN;
      countdownStartTime = millis();
      lcd.clear();
      buttonActive = false;
    }
    if (currentMode == STEP_GOAL && (currentTime - buttonPressStart > 200) && (currentTime - buttonPressStart < LONG_PRESS_TIME)) {
      // Short press to cycle step goal options
      selectedStepGoalIndex = (selectedStepGoalIndex + 1) % 3;
      stepGoal = stepGoalOptions[selectedStepGoalIndex];
      lcd.clear();
      lcd.print("Goal: ");
      lcd.print(stepGoal);
      lcd.print(" steps");
      delay(500);
      buttonActive = false;
    }
  } else {
    buttonActive = false;
  }
}

// --- STEP SIMULATION AND DETECTION ---
void generateSensorData(unsigned long currentTime) {
  float freq = 0.0;
  switch(currentPace) {
    case WALKING: freq = 1.5; break;
    case RUNNING: freq = 3.0; break;
    default: freq = 0.0;
  }
  simulatedAccel = (freq > 0) ? sin(2 * PI * freq * (currentTime / 1000.0)) : 0;
  simulatedAccel += random(-10, 10) / 100.0;
}

void detectSteps(unsigned long currentTime) {
  if (simulatedAccel > stepThreshold && !aboveThreshold) {
    aboveThreshold = true;
    stepCount++;
    if (currentMode == COUNTDOWN && stepGoal > 0) {
      if (stepCount >= stepGoal) {
        totalTime = (currentTime - countdownStartTime) / 1000; // in seconds
        calculateAveragePace();
        currentMode = RESULTS;
        lcd.clear();
      }
    }
    lastStepTime = currentTime;
  }
  if (simulatedAccel < stepThreshold) {
    aboveThreshold = false;
  }
}

void calculateAveragePace() {
  float timeMinutes = totalTime / 60.0;
  averagePace = (stepGoal > 0) ? (timeMinutes / (stepGoal / 1000.0)) : 0; // min per 1000 steps
}

// --- STATE MACHINE FOR PACE ---
void changeState() {
  int randNum = random(100);
  if (randNum < 10) currentPace = RUNNING;
  else if (randNum < 70) currentPace = WALKING;
  else currentPace = STATIONARY;
  stateDuration = random(minStateDuration, maxStateDuration);
  lastStateChange = millis();
}

// --- DISPLAY UPDATES ---
void updateDisplay() {
  lcd.clear();
  switch(currentMode) {
    case NORMAL:
      displayNormalMode();
      break;
    case STEP_GOAL:
      displaySetupGoalMenu();
      break;
    case COUNTDOWN:
      displayCountdownMode();
      break;
    case RESULTS:
      displayResultsMode();
      break;
    default:
      break;
  }
}

void displayNormalMode() {
  lcd.setCursor(0, 0);
  lcd.print("Steps: ");
  lcd.print(stepCount);
  lcd.setCursor(0, 1);
  lcd.print("Pace: ");
  switch(currentPace) {
    case STATIONARY: lcd.print("Stationary"); break;
    case WALKING:    lcd.print("Walking   "); break;
    case RUNNING:    lcd.print("Running   "); break;
  }
}

void displaySetupGoalMenu() {
  lcd.setCursor(0, 0);
  lcd.print("Set Step Goal:");
  lcd.setCursor(0, 1);
  lcd.print(stepGoalOptions[selectedStepGoalIndex]);
  lcd.print(" steps");
}

void displayCountdownMode() {
  lcd.setCursor(0, 0);
  lcd.print("Goal: ");
  lcd.print(stepGoal);
  lcd.print(" steps");
  lcd.setCursor(0, 1);
  lcd.print("Count: ");
  lcd.print(stepCount);
}

void displayResultsMode() {
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(totalTime / 60);
  lcd.print("m ");
  lcd.print(totalTime % 60);
  lcd.print("s");
  lcd.setCursor(0, 1);
  lcd.print("Pace: ");
  lcd.print(averagePace, 1);
  lcd.print(" min/1k");
}

// --- LED UPDATES ---
void updateLEDs() {
  digitalWrite(stationaryLED, currentPace == STATIONARY ? HIGH : LOW);
  digitalWrite(walkingLED,    currentPace == WALKING    ? HIGH : LOW);
  digitalWrite(runningLED,    currentPace == RUNNING    ? HIGH : LOW);
}

// --- SYSTEM CONTROL ---
void resetSystem() {
  stepCount = 0;
  currentMode = MENU;
  menuIndex = 0;
  updateMenuDisplay();
}
