# Nixie Display

This is my modular nixie display/clock.

![Bare clock before case design](./hardware/electronics/clock_no_case.jpg)

It is designed around modules. Each module has:

- 2 nixie tube (with decimal point)
- preceiding colon
- shift register based cathode driver circuit
- any number of modules can be cascaded together

With my boards, the cathode drivers are sandwitched behind the main boards,

The control is based around a Wemos D1 mini and a small 170V power supply. It can also dim the tubes via PWM controlled optogate switching off the high voltage from the tubes.

The recommended input voltage is between 7.5V and 12V.

## Hardware

The PCB design is the most problematic part of the project. I'm proud of the small outline of the NixieBoards, but other than that, they are full with problems. For more details see [its own readme](./hardware/electronics/README.md).

3d printed case coming soon.

## Firmware

The firmware is written in platformIO.

### NixieLib

The nixie driver is written in its own library, what I called NixieLib, and is fully documented in its header file. It supports the following input formats:

- 2 digit number + 4 boolean for the 4 dots (decimal and colon) for each module
- Integer, right adjusted across all modules
- Fixed point number (float input, number of decimals in another input)

### Clock application

The clock gets the current time from NTP, converts it to local time (CET/CEST, modify the winter/summer variables for your timezone) and displays ont the tubes.

It also has OTA enabled and telnet based debugging.

## Future developement

These are the modifications I plan:

- 3d printed case
- simplify NTP/timezone, use ESP's time.h instead of libs
- MQTT support, so it can display anything from my home assistant
- BH1750fvi based dimming
- second display, using Z570M tubes

Otherwise, grab it and modify it to your heart's content, but I probably won't. Pull requests are welcome!
