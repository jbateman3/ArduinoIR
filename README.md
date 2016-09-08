# ArduinoIR

IR bootloader for an arduino atmega328p. Allows wireless and wired uploading of sketches to the arduino. 

Digital pin 11 (PORT B3) is used for the IR Receiver

Sketches upload reliably at 4800 baud (IR Programming). Always keep the transmitter and receiver in close range and line of site.

Sketches upload at 57600 baud for serial programming using the arduino optiboot blootloader

## Ingredients

1x ArduinoIR software

1x Arduino to act as IR programmer

1x Arduino to be programmed over IR

## Instructions
1) Move the contents of the Arduino folder to you Arduino folder in My Documents

2) Select GMC from the boards menu in the Arduino IDE

3) Burn the ArduinoIR bootloader, for help look here: https://www.arduino.cc/en/Tutorial/ArduinoISP

4) Program the Arduino IR Programmer with the "Arduino Programmer" sketch

5) Select the IR frequency your receiver is using

6) Select the COM port of the IR programmer

7) Click Upload

## Arduino IR Programmer
Since your computer does not have an IR emitter a second arduino is required to act as an IR programmer. The sketch for this can be found in the "Arduino Programmer" folder. The circuit diagram for the IR programmer is:

![Arduino IR Circuit Programmer Circuit Diagram](http://i.imgur.com/itQjVQD.png)

## Target Arduino
The Arduino that is going to be programmed needs an IR receiver connected to digital pin 0:

![Arduino IR Circuit Programmer Circuit Diagram](http://i.imgur.com/feCtoD6.png)

## Tip
If you have issues uploading over IR, try turning of "auto reset" and reset the arduino when prompted. 

## TODO
- Flash LED for programming status?
- Make programmer auto-reboot compatible with GMC