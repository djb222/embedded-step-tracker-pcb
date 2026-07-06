#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pin assignments - UPDATED LED PINS
const int stationaryLED = 4;  // CHANGED from 5
const int walkingLED    = 5;  // CHANGED from 6  
const int runningLED    = 6;  // CHANGED from 7
const int button1Pin    = 12; // Single menu/mode button

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

// Step tracking and pace - UPDATED TO USE REAL ADXL
enum PaceState { STATIONARY, WALKING, RUNNING };
PaceState currentPace = STATIONARY;
unsigned long lastStepTime = 0;
int stepCount = 0;

// REAL ADXL VARIABLES - REPLACED SIMULATED DATA
const int xPin = A0;
const int yPin = A1;
const int zPin = A2;
const int stPin = 9; // Self-test pin

// Threshold and calibration variables - FROM WALKING CODE
float threshold = 1.15;        // Adjust this based on testing
float xRest = 2.586;          // Use your measured rest values
float yRest = 4.066;
float zRest = 4.995;


// Step counting variables - FROM WALKING CODE
bool stepDetected = false;
const unsigned long minStepInterval = 300; // Minimum 300ms between steps

// Constants for conversion
const float arduinoVref = 5.0;
const float adxlSupply = 3.3;
const float zeroG = adxlSupply / 2.0;
const float sensitivity = 0.33;

// Timing and state control
unsigned long lastStateChange = 0;
const unsigned long paceUpdateInterval = 2000; // Update pace every 2 seconds

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
void detectSteps(unsigned long currentTime);  // UPDATED TO USE REAL ADXL
void updatePaceFromStepRate(unsigned long currentTime); // NEW FUNCTION
void updateDisplay();
void displayNormalMode();
void displaySetupGoalMenu();
void displayCountdownMode();
void displayResultsMode();
void updateLEDs();
void resetSystem();
void calculateAveragePace();
void runSelfTest();
float readAxisVoltage(int pin);
float readAxisG(int pin);  // FROM WALKING CODE
void calibrateRest();      // FROM WALKING CODE

void setup() {
  pinMode(stPin, OUTPUT);
  digitalWrite(stPin, LOW); // Start with self-test off

  pinMode(stationaryLED, OUTPUT);
  pinMode(walkingLED, OUTPUT);
  pinMode(runningLED, OUTPUT);
  pinMode(button1Pin, INPUT);

  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  
  currentMode = MENU;
  menuIndex = 0;
  updateMenuDisplay();
}

void loop() {
  unsigned long currentTime = millis();
  static unsigned long lastLCDUpdate = 0;

  if (currentMode == MENU) {
    handleMenuButton(currentTime);
    return;
  }

  handleButtons(currentTime);

  if (currentMode == NORMAL || currentMode == STEP_GOAL || currentMode == COUNTDOWN) {
    // REAL STEP DETECTION - REPLACED SIMULATED DATA
    detectSteps(currentTime);
    updatePaceFromStepRate(currentTime);
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
      lcd.print("Calibrating...");
      calibrateRest(); // REAL CALIBRATION
      delay(1500);
      resetSystem();
      break;
    case 3:
      currentMode = SELF_TEST;
      lcd.clear();
      lcd.print("Self-Test...");
      delay(500);
      runSelfTest();
      delay(3000);
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
      currentMode = COUNTDOWN;
      countdownStartTime = millis();
      lcd.clear();
      buttonActive = false;
    }
    if (currentMode == STEP_GOAL && (currentTime - buttonPressStart > 200) && (currentTime - buttonPressStart < LONG_PRESS_TIME)) {
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

// --- REAL STEP DETECTION - REPLACED SIMULATED CODE ---
void detectSteps(unsigned long currentTime) {
  // Read real accelerometer values
  float x_g = readAxisG(xPin);
  float y_g = readAxisG(yPin);
  float z_g = readAxisG(zPin);
  float magRest = 13.7;

  // Calculate raw magnitude
  float magnitude = sqrt(x_g * x_g + y_g * y_g + z_g * z_g);

  // Step detection logic based on rest magnitude
  float stepThreshold = magRest + 0.15; // Adjust 0.15 as needed after testing

  if (magnitude > stepThreshold && !stepDetected &&
      (currentTime - lastStepTime > minStepInterval)) {
    stepCount++;
    stepDetected = true;
    lastStepTime = currentTime;

    // Check if goal reached in countdown mode
    if (currentMode == COUNTDOWN && stepGoal > 0) {
      if (stepCount >= stepGoal) {
        totalTime = (currentTime - countdownStartTime) / 1000;
        calculateAveragePace();
        currentMode = RESULTS;
        lcd.clear();
      }
    }
  }

  // Reset step detection when magnitude drops back near rest
  if (magnitude < magRest + 0.05) { // 0.05 is a small buffer, adjust as needed
    stepDetected = false;
  }

  // Debug output
  Serial.print("Mag: "); Serial.print(magnitude, 3);
  Serial.print(" | Steps: "); Serial.println(stepCount);
}

// --- PACE DETECTION FROM STEP RATE ---
void updatePaceFromStepRate(unsigned long currentTime) {
  static unsigned long lastPaceUpdate = 0;
  static int lastStepCount = 0;
  
  if (currentTime - lastPaceUpdate > paceUpdateInterval) {
    int stepsSinceLastUpdate = stepCount - lastStepCount;
    float stepsPerSecond = stepsSinceLastUpdate / (paceUpdateInterval / 1000.0);
    
    // Classify pace based on step rate
    if (stepsPerSecond < 0.5) {
      currentPace = STATIONARY;
    } else if (stepsPerSecond < 2.0) {
      currentPace = WALKING;
    } else {
      currentPace = RUNNING;
    }
    
    lastPaceUpdate = currentTime;
    lastStepCount = stepCount;
  }
}

// --- REAL ADXL FUNCTIONS - FROM WALKING CODE ---
float readAxisG(int pin) {
  int rawValue = analogRead(pin);
  float voltage = (rawValue * arduinoVref) / 1023.0;
  return (voltage - zeroG) / sensitivity;
}

void calibrateRest() {
  float xSum = 0, ySum = 0, zSum = 0, magSum = 0;
  const int samples = 50;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating...");
  lcd.setCursor(0, 1);
  lcd.print("Keep still!");

  Serial.println("Calibration: Please keep device still...");

  for (int i = 0; i < samples; i++) {
    float x_g = readAxisG(xPin);
    float y_g = readAxisG(yPin);
    float z_g = readAxisG(zPin);
    float mag = sqrt(x_g * x_g + y_g * y_g + z_g * z_g);

    xSum += x_g;
    ySum += y_g;
    zSum += z_g;
    magSum += mag;
    delay(20);
  }

  xRest = xSum / samples;
  yRest = ySum / samples;
  zRest = zSum / samples;
  magRest = magSum / samples;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("X:");
  lcd.print(xRest, 2);
  lcd.print(" Y:");
  lcd.print(yRest, 2);
  lcd.setCursor(0, 1);
  lcd.print("Z:");
  lcd.print(zRest, 2);
  lcd.print(" M:");
  lcd.print(magRest, 2);

  Serial.print("Calibrated rest values - X: "); Serial.print(xRest, 3);
  Serial.print(" Y: "); Serial.print(yRest, 3);
  Serial.print(" Z: "); Serial.print(zRest, 3);
  Serial.print(" | magRest: "); Serial.println(magRest, 3);

  delay(2500); // Show on LCD for 2.5 seconds
}

// --- SELF-TEST ROUTINE ---
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
  float timeMinutes = totalTime / 60.0;
  averagePace = (stepGoal > 0) ? (timeMinutes / (stepGoal / 1000.0)) : 0;
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
