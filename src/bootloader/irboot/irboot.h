/*
 * irboot.h
 *
 * Created: 05/04/2016 16:32:04
 *  Author: Zemborg
 */ 


#ifndef IRBOOT_H_
#define IRBOOT_H_

#include "../GlobalDefines.h"

//Define baud rate for initial connection
#define BAUD   4800
#define MYUBRR (((((CPU_SPEED * 10) / (16L * BAUD)) + 5) / 10) - 1)

// Use a Software UART for IR - allows any pin
#define IR_SOFT_UART
#define IR_UART_PORT   PORTB
#define IR_UART_PIN    PINB
#define IR_UART_DDR    DDRB
#define IR_UART_TX_BIT 5
#define IR_UART_RX_BIT 3
//#define IR_UART_PORT   PORTD
//#define IR_UART_PIN    PIND
//#define IR_UART_DDR    DDRD
//#define IR_UART_TX_BIT 3
//#define IR_UART_RX_BIT 2



//Here we calculate the wait period inside getch().
#define CPU_SPEED F_CPU
#define MAX_CHARACTER_WAIT	15 // allow bytes to be delayed by 15 periods
#define MAX_WAIT_IN_CYCLES ( ((MAX_CHARACTER_WAIT * 8) * CPU_SPEED) / BAUD )

// Atmel Memory page size - from datasheet
#define SPM_PAGESIZE 128
// Highest address of program memory = memory size - bootloader size
#define APP_END 0x7800

//Status LED
#define LED_DDR  DDRB
#define LED_PORT PORTB
#define LED_PIN  PINB
#define LED      PINB5

// Checks the USART Receive Complete bit in UCSR0A
// 1 if There is data in the buffer to be read
#define UART_RECIVE_COMPLETE (UCSR0A & _BV(RXC0))

//Checks the UART for a frame error in the received data
// 1 if there is an error
#define UART_FRAME_ERROR (UCSR0A & _BV(FE0))

//Checks UART if there is a Parity error in the received Data
// 1 if there is an error
#define UART_PARITY_ERROR (UCSR0A & _BV(UPE0))


// Debug defines

//#define DEBUG_IO
//#define DEBUG_UART_OUT
//#define DEBUG_LED
//#define DEBUG_STARTUP_DELAY



#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <avr/wdt.h>


/////////////////////////////////////////////////////////////////////////////////
////////////////////	Function prototypes		////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

//Main function for the IR bootloader
int main_irboot(void) /*__attribute__ ((OS_main))*/;

#ifdef DEBUG_UART_OUT
// Outputs the char to the UART port
void ir_putch(char);
#endif

// Passes back a char from the UART port
// Returns -1 on parity, frame error or timeout else returns 1
int8_t ir_getch(uint8_t *c);

#ifdef DEBUG_LED
// Flashes the LED
// Count = number of times the LED is on
// delayPeriod = time in ms the LED is on
void ir_flash_led(uint8_t count, uint16_t delayPeriod);
#endif

//Programs the temporary buffer to a page in flash
void ir_onboard_program_write(uint32_t page, uint8_t *buf);

// Handles a failed flash upload
// If flash has been partly written then it is erased and the bootloader re-runs
// Else boots the normal program
void ir_UploadFailed();

//Reads UART until we have had at least 4 0xaa bytes then a ':'
//If other data arrives then start the normal app
void ir_waitForStartBit();

#if defined(DEBUG_LED) || defined(DEBUG_STARTUP_DELAY)
// Blocks for the specified time
void ir_delay_ms( int ms );
#endif

//Starts the program if exists else restarts bootloader
void ir_app_start();

// Resets the micro after 15ms
void softRst();

// Waits for a long high time on the serial line to indicate
// a gap between bytes
// Return true if sync to received byte
int sycSerial();

void ir_uartDelay();

void watchdogReset();
void watchdogEnable125();
void watchdogDisable();



#endif /* IRBOOT_H_ */