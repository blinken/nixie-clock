# soviet-tubes

A selection of PCBs and example code to support Soviet-era VFD ("nixie") tubes.
Currently, only the IV-18 is supported, with more planned!

<img src="https://raw.githubusercontent.com/blinken/nixie-clock/main/doc/clock.JPG" width=600>

## IV-18 driver board
A 42x20mm integrated breakout, boost converter and driver. This board connects
to an IV-18 tube and provides 3V or 5V control via the [MAX6921AUI](https://datasheets.maximintegrated.com/en/ds/MAX6921-MAX6931.pdf)
driver chip. The necessary voltage for the VFD tube is generated automatically from the supply voltage.

This board lets you control the IV-18 tube as if it was a TTL-level device.

Pinout:
 * GND
 * +5V: 5V or 3.3V supply (the tube will run slightly dimmer at 3.3V; consider changing R1 for an 0805 0-ohm link to increase filament power, at the expense of slightly shorter tube life)
 * DIN: Connected to the MAX6921. *Serial-Data Input. Data is loaded into the internal shift register on CLK’s rising edge*
 * CLK: Connected to the MAX6921. *Serial-Clock Input. Data is loaded into the internal shift register on CLK’s rising edge*. Claims to support up to 5MHz.
 * LOAD: Drive high to update the display; low to disable the serial bus (equivilent to chip-select). Connected to the MAX6921. *Load Input. Data is loaded transparently from the internal shift register to the output latch while LOAD is high. Data is latched into the output latch on LOAD’s rising edge, and retained while LOAD is low.*
 * BLANK: Drive high to turn off the display. Connected to the MAX6921. *Blanking Input. High forces outputs OUT0 to OUT19 low, without altering the contents of the output latches. Low enables outputs OUT0 to OUT19 to follow the state of the output latches.*
 * DOUT: Data output, for chaining multiple tubes together. Connect to the DIN pin of the next module in line, and connect CLK, LOAD and BLANK together.

<a href="mailto:blinken@gmail.com">Contact me</a> if you'd like a copy.

<img src="https://raw.githubusercontent.com/blinken/nixie-clock/main/doc/driver-breadboard.JPG" width=600>

<img src="https://raw.githubusercontent.com/blinken/nixie-clock/main/doc/driver.JPG" width=600>


## IV-18 breakout
This is a 41x27mm breakout board for the IV-18 nixie tube. It maps every pin on
the tube to a 2x10 2.54mm header.

<img src="https://raw.githubusercontent.com/blinken/nixie-clock/main/doc/breakout-front.JPG" width=600>

<img src="https://raw.githubusercontent.com/blinken/nixie-clock/main/doc/breakout-back.JPG" width=600>


