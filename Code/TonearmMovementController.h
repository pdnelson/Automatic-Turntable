// This library is made specifically for automatic turntable firmware/hardware designed by Patrick Nelson.
// It may have other uses outside of the specific scenarios where it is used here, but that should
// be investigated on an individual basis by whoever stumbles upon this code.
#include "arduino.h"
#include <Stepper.h>
#include <Multiplexer.h>
#include <DcMotor.h>
#include "enums/MotorAxis.h"
#include "enums/HorizontalMovementDirection.h"
#include "enums/VerticalMovementDirection.h"
#include "enums/MovementResult.h"
#include "enums/HorizontalClutchPosition.h"

#ifndef TonearmMovementController_h
#define TonearmMovementController_h

class TonearmMovementController {
    public:

        // Constructor
        TonearmMovementController(
            Multiplexer inputMux,
            Stepper tonearmMotors, 
            uint8_t motorSelectPin, // arduino output pin
            DcMotor horizontalClutch, 
            uint8_t verticalLowerLimit, // mux input pin
            uint8_t verticalUpperLimit  // mux input pin
        );

        // Move the tonearm horizontally until it reaches the destination sensor.
        // destinationSensor - The sensor that the tonearm is traveling to. This sensor MUST be from the multiplexer passed through the constructor.
        // timeout - The number of steps that should be taken before an error occurs.
        // calibration - The number of steps that should be taken after the sensor is reached.
        // speed - The speed, in RPM, that the motor moving the tonearm should spin.
        MovementResult moveTonearmHorizontally(uint8_t destinationSensor, unsigned int timeout, int calibration, uint8_t speed);
        
        // Move the tonearm vertically until it reaches the destination limit switch.
        // direction - The direction that the tonearm should be moving.
        // timeout - The number of steps that should be taken before an error occurs.
        // speed - The speed, in RPM, that the motor moving the tonearm should spin.
        MovementResult moveTonearmVertically(VerticalMovementDirection direction, unsigned int timeout, uint8_t speed);
        
        // Move the tonearm horizontally by the given step count. This is a blind movement, meaning there is no check 
        // at the end that the tonearm successfully moved all steps.
        // steps - The number of steps the tonearm should move.
        // speed - The speed, in RPM, that the motor moving the tonearm should spin.
        // direction - The direction the tonearm should be moving.
        void horizontalRelativeMove(uint16_t steps, uint8_t speed, HorizontalMovementDirection direction);

        // Set the value for how long it is expected that the clutch will take to engage or disengage from the horizontal gears.
        void setClutchEngagementMs(uint16_t ms);

        // Set the position of the horizontal clutch, while also accounting for the delay and shutting off the motor at the end of the movement.
        void setClutchPosition(HorizontalClutchPosition position);

        // Set the top motor speed that the horizontal motor can travel. This is only used after the horizontal sensor has been found and
        // we are applying the calibration relative move.
        void setTopMotorSpeed(uint8_t topSpeed);

    private:        
        // Set all pins that the motor is using to LOW, as well as the motor demultiplexer.
        void releaseCurrentFromMotors();

        // Multiplexer that we are reading all sensor values from
        Multiplexer inputMux;

        // This represents either the vertical or horizontal stepper motor, denoted by the motorSelectPin.
        Stepper tonearmMotors;

        // This is the pin that we use to select the vertical or horizontal stepper motors for movement. Only one
        // motor can be moving at a time.
        uint8_t motorSelectPin;

        // This is the pin that the horizontal solenoid is attached to, which will engage the gears for movement.
        uint8_t horizontalSolenoidPin;

        // We need to know which mux pins are the upper and lower limit so we know which direction the tonearm should move
        // when we want to go to a particular sensor.
        uint8_t verticalLowerLimit;
        uint8_t verticalUpperLimit;

        // This is used to engage and disengage the gears that drive the horizontal movement of the tonearm.
        DcMotor horizontalClutch;

        // How long it is estimated that the clutch takes to engage or disengage.
        uint16_t clutchEngagementMs;

        // The top motor speed that the horizontal motor can travel. This is only used after the horizontal sensor has been found and
        // we are applying the calibration relative move.
        uint8_t topMotorSpeed;
};

#endif
