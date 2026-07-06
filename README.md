# Embedded Step-Tracker PCB Software

## Overview

This repository contains embedded software developed for a custom step-tracker PCB in Embedded Mechatronics Studio.

The system used an ADXL335 accelerometer, Arduino Uno, LCD display, push button interface, LED pace indicators, and custom PCB circuitry. The software handled step detection, menu navigation, calibration, self-test, step-goal countdown mode, LCD updates, and LED output logic.

## My Contribution

This was a university group project. My main contribution focused on the embedded software and parts of the PCB interface/electrical design.

I contributed to:

- Developing the final Arduino software for the PCB.
- Reading ADXL335 accelerometer data through Arduino analog inputs.
- Implementing step detection using acceleration magnitude and threshold logic.
- Building LCD menu navigation using a single push button with short/long press behaviour.
- Implementing step-goal countdown mode and results display.
- Implementing pace classification for stationary, walking, and running states.
- Driving LED output indicators based on user pace.
- Implementing ADXL335 self-test and calibration routines.
- Testing and debugging the software using breadboard and assembled PCB setups.

## Technologies Used

- Arduino C/C++
- Arduino Uno
- ADXL335 accelerometer
- I2C LCD
- Push button interface
- LED driver outputs
- Embedded software
- PCB testing and validation

## Key Files

```text
src/
└── finalDemoCode.ino              # Final integrated demo code

prototypes/
├── ADXLTestCode.ino               # Basic ADXL335 voltage/acceleration reading
├── ADXLTestWalkingCode.ino        # Early walking/step detection prototype
├── EMS_Code1BasicComponentTest.ino # Initial component/menu/LED test code
├── SelfTestSerialMonitor.ino      # ADXL335 self-test serial monitor test
├── TestingFinal1.ino              # Late-stage integration test
└── TestFinal2.ino                 # Countdown/step-goal testing version
