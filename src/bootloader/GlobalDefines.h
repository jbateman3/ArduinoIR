/*
 * GlobalDefines.h
 *
 * Created: 05/04/2016 17:03:47
 *  Author: Zemborg
 */ 


#ifndef GLOBALDEFINES_H_
#define GLOBALDEFINES_H_


// Oscillator speed for the CPU
#define F_CPU 16000000

// How many times to flash the led on bootup when optiboot runs
// No usable LED on GMC
#define LED_START_FLASHES 0

// Baud rate optiboot uses for programming
// Using fastest working software serial baud rate
#define BAUD_RATE 115200 
//#define BAUD_RATE 38400 
//#define BAUD_RATE 57600 


#endif /* GLOBALDEFINES_H_ */