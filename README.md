# Automatic Turntable Introduction
This is a semi-automatic turntable that plays 7" records! The automatic capabilities will be powered by an Arduino Uno, 2 stepper motors, and a bunch of sensors.
Some planned features of this turntable include:
- Stereo RCA outputs for a receiver
- Able to fit any commercial cartridge
- 45 and 33-RPM speeds
- Semi-automatic
  - Returns tonearm to "home" position automatically after a record is finished
  - Other buttons that contain other pre-defined routines (play and pause) that the user must initiate
- Standard PC 3-prong female plug in the back to allow hookup to 120v or 230v households

The actual "turntable" part is yet to be designed; right now, I'm just working on the automatic functionality.

# Layout
The layout is yet to be designed, but a very general prototype can be seen in Figure 1.

![image](https://cdn.discordapp.com/attachments/625801308854812684/848020821326037032/20210528_221130.jpg)
Figure 1. Prototype layout of the turntable.

# Inputs and routines
The user has a total of four inputs they can use. Most of these functions must be initiated by the user by either pressing a button or flipping a switch, though homing can also be done automatically, which will be explained in more detail later on. Routine interrupt is currently not planned. This means that while one routine is running, none of the others can be executed for the duration of the currently-running routine.

## Automatic/manual switch
This is a 3-way switch with the center position being "off." Flipping the switch to the "up" position will set the turntable to automatic, while "down" will set it to manual. The turntable will automatically be homed upon flipping the switch to "automatic." Flipping it to "manual" will home the vertical axis, which will set the tonearm down in place where it currently is. The reason for this inclusion is to account for us not knowing what position the tonearm will be in when the device is turned on.

As soon as this switch is flipped to "manual" or "automatic," the software setup procedure begins.

## Play/Home button
The "play/home" button will pick the tonearm up from any point, and either drop it at the beginning of a record, or at the home position. If the tonearm is past the play position (i.e. on a record), then the button will home the tonearm. Otherwise, the button will drop it at the beginning of a record. These positions are determined using a slotted optical sensor.

## Pause button
The pause button will lift the tonearm up until the pause limit switch becomes "high." When the pause button is pressed again, the tonearm will be gently set down on the record.

# Current pin usage
- Digital
  - 0: UNUSED
  - 1: UNUSED
  - 2: Motor Demultiplexer input 0
  - 3: UNUSED (reserved for possible future PWM usage)
  - 4: Motor Demultiplexer input 1
  - 5: UNUSED (reserved for possible future PWM usage)
  - 6: UNUSED (reserved for possible future PWM usage)
  - 7: Motor Demultiplexer input 2
  - 8: Motor Demultiplexer input 3
  - 9: Motor Demultiplexer select 0, 1, 2, 3
  - 10: Horizontal gearing solenoid
  - 11: Movement status LED
  - 12: Pause status LED
  - 13: Multiplexer output

- Analog
  - A0: Input Multiplexer Selector A
  - A1: Input Multiplexer Selector B
  - A2: Input Multiplexer Selector C
  - A3: UNUSED
  - A4: UNUSED
  - A5: UNUSED

- Input Multiplexer
  - IN 0: Play/Home button
  - IN 1: Pause button
  - IN 2: Vertical upper (pause) limit
  - IN 3: Vertical lower (home) limit
  - IN 4: Horizontal "home" optical sensor
  - IN 5: Horizontal "play" optical sensor
  - IN 6: Horizontal "pickup" optical sensor
  - IN 7: Auto/Manual mode switch

- Motor Demultiplexer
  - OUT 0A: Vertical stepper motor pin 1
  - OUT 0B: Horizontal stepper motor pin 1
  - OUT 1A: Vertical stepper motor pin 2
  - OUT 1B: Horizontal stepper motor pin 2
  - OUT 2A: Vertical stepper motor pin 3
  - OUT 2B: Horizontal stepper motor pin 3
  - OUT 3A: Vertical stepper motor pin 4
  - OUR 3B: Horizontal stepper motor pin 4

# Parts list (so far)
## Electrical parts
- Arduino Uno
- Mean well RS-15-5 5V 3A power supply
- 1x ADA2776 5v solenoid
- 2x 28BYJ-48 stepper motors
- 2x ULN2003 stepper motor drivers
- 4x slotted optical sensors
- 2x micro limit switches
- breadboards (as many as you need)
- 2x LED lights (any color; these are to indicate movement and pause)
- 26 AWG stranded wire
- 3x blue cherry mx Mechanical Keyswitch
- 1x HYEJET-01 motor
- 2x 1N4007 diodes
- 2x TIP120 transistors
- 1x CD4051BE multiplexer

## Mechanical parts
- 1x 3/16" steel rods, 5" long
- 1x 3/16" steel rods, 4" long
- 1x 1.8"x1.8" key stock, ~3" long
- many screws; 4-40 threading, 0.183" diameter head, 1/4" long
- a few more screws; 4-40 threading, 0.183" diameter head, 3/8" long
- many square nuts; 4-40 threading, 1/4" diameter, 3/32" thick
- washers; 18-8 Stainless Steel Washer for Number 10 Screw Size, 0.203" ID, 0.438" OD
- yet more screws; 2-56 threading, 1/2" long
- 5x5mm square nuts; 2-56 threading
- washers for no. 2 screw size, 0.094" inner diameter

## Miscellaneous parts
- 10x10mm heat sinks
- Thermal paste
