# Arduino Programmer
Source code to be programmed onto an arduino to act as a programmer. 

Pin 11 is used to control the transmit LED. It is recommended to use lots of bright IR LEDs in order to bet the best connection. Use a transistor to power the LEDs as the arduino pin wont provide enough current.

Based on a modified version of the softwareSerial library and FrequencyTimer2 library.

Forwards data received from the computer over IR, data is sent from the computer in sets of 100 bytes, the programmer must request more data.



# Bootloader

The bootloader for the arduino, Receives serial over IR. Tries to boot the application code on any sign of trouble as to avoid being stuck in the booloader. Communication defaults to 4800 baud, but is re-negotiated  after connection. Data is sent with check sums to identify any errors.

# Example Sketch

A horrible uncommented blink example that automatically switches to the bootloader when a reprogram command is sent over IR

# Programmer

VB program on the computer to send the compiled hex file over to the arduino IR programmer. Command line program, use -h for help