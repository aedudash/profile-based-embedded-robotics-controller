# Profile-Based Embedded Robotics Controller

An embedded access-control and robotics system developed using an ATmega328PB microcontroller, AVR C, and AVR assembly.

The controller combines persistent user authentication with profile-based hardware permissions. Users authenticate through a UART terminal before gaining access to joystick-controlled servo movement, while each profile is assigned a different allowable range of motion. The system also incorporates an interrupt-driven emergency lockout, visual status indication, audible feedback, and administrative credential management.

## Project Overview

This project was independently designed and developed as a Microprocessors II project to integrate multiple embedded-system concepts into a single working application.

Rather than allowing every user identical control of the hardware, the system uses three selectable profiles:

- **Admin**
- **User 1**
- **User 2**

Each profile has its own password stored in EEPROM and its own permitted servo-control range.

After successful authentication, the microcontroller enables the system's control functions. Joystick input is sampled through the ADC and converted into a PWM command for the servo motor. The resulting command is then limited according to the currently authenticated user's permissions.

## Key Features

- Three independent user profiles
- Persistent password storage using EEPROM
- Password creation for uninitialized profiles
- Masked password entry through UART
- Three-attempt authentication lockout
- Administrative password-reset functionality
- Profile-based servo movement permissions
- Analog joystick input using the ADC
- 50 Hz servo PWM generated with Timer1
- Interrupt-driven ADC sampling
- External-interrupt emergency input
- Hardware reset and reauthentication following emergency lockout
- Red and green status indication
- Audible buzzer feedback
- Low-level peripheral configuration in AVR C
- Custom timing routine implemented in AVR assembly

## Profile-Based Control

The authenticated profile determines how much of the servo's available range the user is permitted to command.

| Profile | Authentication | Approximate Servo Access |
| --- | --- | --- |
| Admin | Required | Full range (~180°) |
| User 1 | Required | Limited range (~90°) |
| User 2 | Required | Limited range (~45°) |

The ADC continuously reads the joystick position and generates a corresponding servo command. The firmware then clamps that command to the minimum and maximum values allowed for the selected profile.

This allows authentication to control not only whether the system can be used, but also **what the authenticated user is authorized to do**.

## System Operation

```text
Power On
   |
   v
Select User Profile
   |
   v
Check EEPROM for Existing Password
   |
   +---- No ----> Create and Store Password
   |
   v
Authenticate User
   |
   +---- Failure ----> Retry ----> Lockout After 3 Attempts
   |
   v
Access Granted
   |
   v
Enable ADC + PWM + Emergency Interrupt
   |
   v
Joystick Controls Servo
   |
   v
Apply Profile-Specific Motion Limits
   |
   v
Emergency Input Triggered?
   |
   +---- Yes ----> Revoke Access
                   Disable Control
                   Activate Alert
                   Require Hardware Reset
                   Return to Authentication
