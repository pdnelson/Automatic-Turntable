#include <Stepper.h>
#include <Multiplexer.h>
#include "headers/AutomaticTurntable.h"
#include "enums/MultiplexerInput.h"
#include "enums/MovementResult.h"
#include "enums/AutoManualSwitchPosition.h"
#include "enums/RecordSize.h"
#include "TonearmMovementController.h"

// Used for 7-segment display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

// Used for calibration storage
#include <EEPROM.h>

//#define SERIAL_SPEED 115200

#define STEPS_PER_REVOLUTION 2048

// These motor pins channel into four 2-channel demultiplexers, so that either the vertical or horizontal motor receives
// the pulses. Only one of these motors can ever be moving at once.
#define MOTOR_PIN1 10
#define MOTOR_PIN2 9
#define MOTOR_PIN3 8
#define MOTOR_PIN4 7
Stepper TonearmMotor = Stepper(STEPS_PER_REVOLUTION, MOTOR_PIN4, MOTOR_PIN2, MOTOR_PIN3, MOTOR_PIN1);

// This is the pin used to select which motor we are moving, using the demultiplexer.
#define MOTOR_AXIS_SELECTOR 11

// This is used to engage the horizontal gears for movement. This is needed so that the gears aren't engaged
// when a record is playing or any other times, otherwise the record would not be able to move the tonearm
// very well...
#define HORIZONTAL_GEARING_SOLENOID A7

// Indicator lights so we can tell what the turntable is currently doing.
#define MOVEMENT_STATUS_LED 12
#define PAUSE_STATUS_LED 13

// These are the selector pins for the multiplexer that is used to handle all inputs.
#define MUX_OUTPUT 6
#define MUX_SELECTOR_A 2
#define MUX_SELECTOR_B 3
#define MUX_SELECTOR_C 4
#define MUX_SELECTOR_D 5
Multiplexer mux = Multiplexer(MUX_OUTPUT, MUX_SELECTOR_A, MUX_SELECTOR_B, MUX_SELECTOR_C, MUX_SELECTOR_D);

TonearmMovementController tonearmController = TonearmMovementController(
  mux,
  MOTOR_PIN1,
  MOTOR_PIN2,
  MOTOR_PIN3,
  MOTOR_PIN4,
  TonearmMotor,
  MOTOR_AXIS_SELECTOR,
  HORIZONTAL_GEARING_SOLENOID,
  MultiplexerInput::VerticalLowerLimit,
  MultiplexerInput::VerticalUpperLimit
);

// The motors used in this project are 28BYJ-48 stepper motors, which I've found to cap at 11 RPM 
// before becoming too unreliable. 8 or 9 I've found to be a good balance for speed and reliability at 5v DC.
#define DEFAULT_MOVEMENT_RPM 8

// The horizontal motor needs to run a bit faster because of the gearing ratio
#define HORIZONTAL_HOME_MOVEMENT_RPM 9

// These are calibration values used to position the tonearm approximately where it needs to go. These values are finished off
// by adding the potentiometer values.
#define STEPS_FROM_PLAY_SENSOR_HOME 100

// These are calibration values set by the user to tell the tonearm how many steps to go past the "home" reference optical
// sensor.
uint16_t calibration7Inch = 0;
uint16_t calibration10Inch = 0;
uint16_t calibration12Inch = 0;

// These are the values that determine the slowest that each calibration value may increment/decrement when holding a button
#define CALIBRATION_HOLD_CHANGE_MS 500 // The interval between value updates
#define CALIBRATION_HOLD_CHANGE 5 // The number of updates before the change MS decreases

#define CALIBRATION_7IN_EEPROM_START_ADDRESS 0
#define CALIBRATION_10IN_EEPROM_START_ADDRESS 2
#define CALIBRATION_12IN_EEPROM_START_ADDRESS 4

// These are timeouts used for error checking, so the hardware doesn't damage itself.
// Essentially, if the steps exceed this number and the motor has not yet reached its
// destination, an error has occurred.
#define VERTICAL_MOVEMENT_TIMEOUT_STEPS 1000
#define HORIZONTAL_MOVEMENT_TIMEOUT_STEPS 3000

// The 7-segment display is used for the following:
// - Displaying the speed that the turntable is spinning.
// - If the user is pressing the calibration button, displaying the current sensor's calibration value.
// - Error codes if a movement fails.
Adafruit_7segment sevSeg = Adafruit_7segment();

// These fields are so we aren't writing to the 7-segment display so often
double lastSevSegValue = 0.0;

// All of these fields are used to calculate the speed that the turntable is spinning.
#define SPEED_SENSOR A3
volatile unsigned long currMillisSpeed = millis();
volatile unsigned long lastMillisSpeed = currMillisSpeed;
volatile double currSpeed;

void setup() {
  //Serial.begin(SERIAL_SPEED);

  pinMode(MOVEMENT_STATUS_LED, OUTPUT);
  pinMode(PAUSE_STATUS_LED, OUTPUT);

  pinMode(SPEED_SENSOR, INPUT);
  attachInterrupt(digitalPinToInterrupt(SPEED_SENSOR), calculateTurntableSpeed, RISING);

  mux.setDelayMicroseconds(10);

  sevSeg.begin(0x70);
  sevSeg.print(0.0);
  sevSeg.writeDisplay();

  // Load calibration values from EEPROM
  calibration7Inch = (EEPROM.read(CALIBRATION_7IN_EEPROM_START_ADDRESS) << 8 ) + EEPROM.read(CALIBRATION_7IN_EEPROM_START_ADDRESS + 1);
  calibration10Inch = (EEPROM.read(CALIBRATION_10IN_EEPROM_START_ADDRESS) << 8 ) + EEPROM.read(CALIBRATION_10IN_EEPROM_START_ADDRESS + 1);
  calibration12Inch = (EEPROM.read(CALIBRATION_12IN_EEPROM_START_ADDRESS) << 8 ) + EEPROM.read(CALIBRATION_12IN_EEPROM_START_ADDRESS + 1);

  // Validate that calibration values are within the accepted range
  if(calibration7Inch > 2500) calibration7Inch = 0;
  if(calibration10Inch > 2500) calibration10Inch = 0;
  if(calibration12Inch > 2500) calibration12Inch = 0;

  // Begin startup light show
  delay(100);
  digitalWrite(MOVEMENT_STATUS_LED, HIGH);
  delay(100);
  digitalWrite(PAUSE_STATUS_LED, HIGH);
  delay(100);
  digitalWrite(PAUSE_STATUS_LED, LOW);
  delay(100);
  digitalWrite(MOVEMENT_STATUS_LED, LOW);
  // End startup light show

  MovementResult currentMovementStatus = MovementResult::None;

  // If the turntable is turned on to "automatic," then home the whole tonearm if it is not already home.
  if(mux.readDigitalValue(MultiplexerInput::AutoManualSwitch) == AutoManualSwitchPosition::Automatic && 
    !mux.readDigitalValue(MultiplexerInput::HorizontalHomeOrPlayOpticalSensor)) {
    MovementResult currentMovementStatus = homeRoutine();
  }

  // Otherwise, we only want to home the vertical axis if it is not already homed, which will drop the tonearm in its current location.
  else if(!mux.readDigitalValue(MultiplexerInput::VerticalLowerLimit)) {
      currentMovementStatus = pauseOrUnpause();
  }

  if(currentMovementStatus != MovementResult::Success && currentMovementStatus != MovementResult::None) {
    setErrorState(currentMovementStatus);
  }
}

void loop() {
  // We always want to make sure the solenoid is not being powered when a command is not executing. There are some bugs that are mostly out of my control
  // that may cause the solenoid to become HIGH, for example, unplugging the USB from the Arduino can sometimes alter the state of the software.
  digitalWrite(HORIZONTAL_GEARING_SOLENOID, LOW);

  monitorCommandButtons();
  monitorSevenSegmentInput();
}

// When a command button is pressed (i.e. Home/Play, or Pause/Unpause), then its respective command will be executed.
void monitorCommandButtons() {
  MovementResult currentMovementStatus = MovementResult::None;

  if(mux.readDigitalValue(MultiplexerInput::PauseButton)) {
    currentMovementStatus = pauseOrUnpause();
  }
  
  else if(mux.readDigitalValue(MultiplexerInput::PlayHomeButton) ||
   (!mux.readDigitalValue(MultiplexerInput::HorizontalPickupOpticalSensor) && 
     mux.readDigitalValue(MultiplexerInput::AutoManualSwitch))) {

    // If the tonearm is past the location of the home sensor, then this button will home it. Otherwise, it will execute
    // the play routine.
    if(mux.readDigitalValue(MultiplexerInput::HorizontalHomeOrPlayOpticalSensor)) 
      currentMovementStatus = playRoutine();
    else 
      currentMovementStatus = homeRoutine();
  }

  if(currentMovementStatus != MovementResult::Success && currentMovementStatus != MovementResult::None) {
    setErrorState(currentMovementStatus);
  }
}

// This will allow the 7-segment display to either display the current turntable speed from the input of the 
// speed sensor, or one of the three calibration values.
void monitorSevenSegmentInput() {

  double newValue;

  // If the calibration button is being pressed, enter "calibration" mode so the user can view or modify
  // values.
  if(mux.readDigitalValue(MultiplexerInput::DisplayCalibrationValue)) {
    calibrationSettingLoop();
  }

  // If 3 seconds elapse without a speed sensor interrupt, we can assume that the turntable has stopped.
  if(millis() - currMillisSpeed > 3000 && currSpeed > 0.0) {
    newValue = 0.0;
  }
  else newValue = currSpeed;

  updateSevenSegmentDisplay(newValue);
}

// This loop is what is executed while in "calibration" mode. This implementation allows the user to switch between
// all three calibrations while having the button held
void calibrationSettingLoop() {

  uint16_t calibrationDisplayValue = 0;

  // Keep track of the old values so we can decide which ones to save in the EEPROM
  uint16_t old7In = calibration7Inch;
  uint16_t old10In = calibration10Inch;
  uint16_t old12In = calibration12Inch;


  while(mux.readDigitalValue(MultiplexerInput::DisplayCalibrationValue)) {
    if(mux.readDigitalValue(MultiplexerInput::PauseButton) && calibrationDisplayValue < 2499) {
      switch(getActiveRecordSize()) {
        case RecordSize::Rec7Inch: calibrationDisplayValue = calibration7Inch++; break;
        case RecordSize::Rec10Inch: calibrationDisplayValue = calibration10Inch++; break;
        case RecordSize::Rec12Inch: calibrationDisplayValue = calibration12Inch++;
      }
    }
    else if(mux.readDigitalValue(MultiplexerInput::PlayHomeButton) && calibrationDisplayValue > 1) {
      switch(getActiveRecordSize()) {
        case RecordSize::Rec7Inch: calibrationDisplayValue = calibration7Inch--; break;
        case RecordSize::Rec10Inch: calibrationDisplayValue = calibration10Inch--; break;
        case RecordSize::Rec12Inch: calibrationDisplayValue = calibration12Inch--;
      }
    }
    else {
      calibrationDisplayValue = getActiveSensorCalibration();
    }

    updateSevenSegmentDisplay((double)calibrationDisplayValue);
  }

  updateCalibrationEEPROMValues(old7In, old10In, old12In);
}

void updateSevenSegmentDisplay(double newValue) {
  // Only re-write the display if the number will be different
  if(newValue != lastSevSegValue) {
    sevSeg.print(newValue);
    sevSeg.writeDisplay();
    lastSevSegValue = newValue;
  }
}

// Move the tonearm clockwise to the play sensor
// This is a multi-movement routine, meaning that multiple tonearm movements are executed. If one of those movements fails, the
// whole routine is aborted.
MovementResult playRoutine() {
  digitalWrite(MOVEMENT_STATUS_LED, HIGH);
  digitalWrite(PAUSE_STATUS_LED, LOW);

  MovementResult result = MovementResult::None;

  int calibration = getActiveSensorCalibration();

  result = tonearmController.moveTonearmVertically(MultiplexerInput::VerticalUpperLimit, VERTICAL_MOVEMENT_TIMEOUT_STEPS, DEFAULT_MOVEMENT_RPM);
  if(result != MovementResult::Success) return result;

  result = tonearmController.moveTonearmHorizontally(MultiplexerInput::HorizontalHomeOrPlayOpticalSensor, HORIZONTAL_MOVEMENT_TIMEOUT_STEPS, calibration, DEFAULT_MOVEMENT_RPM);
  if(result != MovementResult::Success) return result;

  result = tonearmController.moveTonearmVertically(MultiplexerInput::VerticalLowerLimit, VERTICAL_MOVEMENT_TIMEOUT_STEPS, 3);
  if(result != MovementResult::Success) return result;

  digitalWrite(HORIZONTAL_GEARING_SOLENOID, LOW);

  digitalWrite(MOVEMENT_STATUS_LED, LOW);
  
  return result;
}

// Move the tonearm counterclockwise to the home sensor.
// This is a multi-movement routine, meaning that multiple tonearm movements are executed. If one of those movements fails, the
// whole routine is aborted.
MovementResult homeRoutine() {
  digitalWrite(MOVEMENT_STATUS_LED, HIGH);
  digitalWrite(PAUSE_STATUS_LED, LOW);

  MovementResult result = MovementResult::None;

  result = tonearmController.moveTonearmVertically(MultiplexerInput::VerticalUpperLimit, VERTICAL_MOVEMENT_TIMEOUT_STEPS, DEFAULT_MOVEMENT_RPM);
  if(result != MovementResult::Success) return result;

  result = tonearmController.moveTonearmHorizontally(MultiplexerInput::HorizontalHomeOrPlayOpticalSensor, HORIZONTAL_MOVEMENT_TIMEOUT_STEPS, STEPS_FROM_PLAY_SENSOR_HOME, HORIZONTAL_HOME_MOVEMENT_RPM);
  if(result != MovementResult::Success) return result;

  result = tonearmController.moveTonearmVertically(MultiplexerInput::VerticalLowerLimit, VERTICAL_MOVEMENT_TIMEOUT_STEPS, DEFAULT_MOVEMENT_RPM);
  if(result != MovementResult::Success) return result;

  digitalWrite(HORIZONTAL_GEARING_SOLENOID, LOW);

  digitalWrite(MOVEMENT_STATUS_LED, LOW);

  return result;
}

// This is the pause routine that will lift up the tonearm from the record until the user "unpauses" by pressing the
// pause button again
MovementResult pauseOrUnpause() {
  digitalWrite(MOVEMENT_STATUS_LED, LOW);
  digitalWrite(PAUSE_STATUS_LED, HIGH);

  MovementResult result = MovementResult::None;

  // If the vertical lower limit is pressed (i.e., the tonearm is vertically homed), then move it up
  if(mux.readDigitalValue(MultiplexerInput::VerticalLowerLimit)) {
    result = tonearmController.moveTonearmVertically(MultiplexerInput::VerticalUpperLimit, VERTICAL_MOVEMENT_TIMEOUT_STEPS, DEFAULT_MOVEMENT_RPM);
  }

  // Otherwise, just move it down and then shut off the LED
  else {
    uint8_t tonearmSetRpm = 0;

    // If the tonearm is hovering over home position, then just go down at default speed
    if(mux.readDigitalValue(MultiplexerInput::HorizontalHomeOrPlayOpticalSensor)) {
      tonearmSetRpm = DEFAULT_MOVEMENT_RPM;
    }

    // Otherwise, set it down carefully
    else tonearmSetRpm = 3;

    result = tonearmController.moveTonearmVertically(MultiplexerInput::VerticalLowerLimit, VERTICAL_MOVEMENT_TIMEOUT_STEPS, tonearmSetRpm);

    digitalWrite(PAUSE_STATUS_LED, LOW);
  }

  return result;
}

// Returns the calibration step offset for the given sensor.
// The returned value will be the number of steps (clockwise or counterclockwise) that the horizontal motor should move.
uint16_t getActiveSensorCalibration() {
  switch(getActiveRecordSize()) {
    case RecordSize::Rec7Inch:
      return calibration7Inch;
    case RecordSize::Rec10Inch:
      return calibration10Inch;
    case RecordSize::Rec12Inch:
      return calibration12Inch;
  }
}

RecordSize getActiveRecordSize() {
  // If only RecordSizeSelector1 is HIGH (or BOTH RecordSelector1 and 2 are HIGH), we are using the 7" sensor
  if(mux.readDigitalValue(MultiplexerInput::RecordSizeSelector1))
    return RecordSize::Rec7Inch; 

  // If only RecordSizeSelector2 is HIGH, we are using the 12" sensor
  else if (mux.readDigitalValue(MultiplexerInput::RecordSizeSelector2))
    return RecordSize::Rec12Inch;

  // If NEITHER RecordSizeSelectors are HIGH, we are using the 10" sensor
  else return RecordSize::Rec10Inch;
}

// This will update all numbers that have changed
void updateCalibrationEEPROMValues(uint16_t old7In, uint16_t old10In, uint16_t old12In) {
  // We only try to write each value to the EEPROM if it has been changed.
  if(old7In != calibration7Inch) {
    EEPROM.write(CALIBRATION_7IN_EEPROM_START_ADDRESS, calibration7Inch >> 8);
    EEPROM.write(CALIBRATION_7IN_EEPROM_START_ADDRESS + 1, calibration7Inch  & 0xFF);
  }

  if(old10In != calibration10Inch) {
    EEPROM.write(CALIBRATION_10IN_EEPROM_START_ADDRESS, calibration10Inch >> 8);
    EEPROM.write(CALIBRATION_10IN_EEPROM_START_ADDRESS + 1, calibration10Inch  & 0xFF);
  }

  if(old12In != calibration12Inch) {
    EEPROM.write(CALIBRATION_12IN_EEPROM_START_ADDRESS, calibration12Inch >> 8);
    EEPROM.write(CALIBRATION_12IN_EEPROM_START_ADDRESS + 1, calibration12Inch  & 0xFF);
  }

  // If any values were changed, blink the play LED once to signify that the data was saved
  if(old12In != calibration12Inch || old10In != calibration10Inch || old7In != calibration7Inch) {
    digitalWrite(MOVEMENT_STATUS_LED, HIGH);
    delay(250);
    digitalWrite(MOVEMENT_STATUS_LED, LOW);
  }
}

// Each time the interrupt calls this function, the current milliseconds are polled, and compared against the last polling
// to calculate the speed the turntable is spinning on each rotation. This calculation is always occurring, even if the speed
// is not being displayed.
// This is attached to an interrupt because it is a time-sensitive operation, and having to wait for other code to finish executing
// would cause this to return inaccurate values.
void calculateTurntableSpeed() {
  currMillisSpeed = millis();
  currSpeed = 60000 / (double)(currMillisSpeed - lastMillisSpeed);
  lastMillisSpeed = currMillisSpeed;
}

// This stops all movement and sets the turntable in an error state to prevent damage.
// This will be called if a motor stall has been detected.
void setErrorState(MovementResult movementResult) {
  digitalWrite(PAUSE_STATUS_LED, HIGH);
  digitalWrite(MOVEMENT_STATUS_LED, HIGH);

  sevSeg.clear();
  sevSeg.writeDigitNum(0, movementResult, false);
  sevSeg.writeDisplay();

  // Wait for the user to press the Play/Home or Pause/Unpause buttons to break out of the error state
  while(!mux.readDigitalValue(MultiplexerInput::PlayHomeButton) && !mux.readDigitalValue(MultiplexerInput::PauseButton)) { delay(1); }

  sevSeg.clear();
  sevSeg.writeDisplay();
}