/*
* BootloaderIR.c
*
*	Based of the sparkfun custom arduino bootloader:
*	https://www.sparkfun.com/tutorials/122
*
*	Only 1216 bytes with -Os
*	
*	Program Flow:
*	1) Check if program already exists in flash
*	2) Wait for data while receiving 0xaa bytes
*	3) First 2 bytes are new UART config
*	4) Wait for more data while reciving 0xaa bytes
*	5) Read page to buffer
*	6) Write page to flash
*	7) Repeat until finish command
*
*	The bootloder tries to load the flash program at any issue to avoid being stuck in the booloader
*
*	TODO: Fix / Test more receiving data without a parity bit
*
*
* Created: 20/03/2016 10:57:54
* Author : zemborg
*/

// Oscillator speed for the CPU
#define F_CPU 16000000

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

//Define baud rate for initial connection
#define BAUD   4800 
#define MYUBRR (((((CPU_SPEED * 10) / (16L * BAUD)) + 5) / 10) - 1)

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


#ifdef DEBUG_IO

// Set the 4 test pins high
#define SET_TESTPIN1 PORTD |= 0b00010000;
#define SET_TESTPIN2 PORTD |= 0b00100000;
#define SET_TESTPIN3 PORTD |= 0b01000000;
#define SET_TESTPIN4 PORTD |= 0b10000000;

// Set the 4 test pins low
#define CLEAR_TESTPIN1 PORTD &= 0b11101111;
#define CLEAR_TESTPIN2 PORTD &= 0b11011111;
#define CLEAR_TESTPIN3 PORTD &= 0b10111111;
#define CLEAR_TESTPIN4 PORTD &= 0b01111111;

// Used to know if the pins are high so they can be toggled
int pinstate = 1;
int pinstate2 = 1;

#endif

/////////////////////////////////////////////////////////////////////////////////
////////////////////	Function prototypes		////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_UART_OUT
 // Outputs the char to the UART port
void putch(char);
#endif

// Passes back a char from the UART port
// Returns -1 on parity, frame error or timeout else returns 1
int8_t getch(uint8_t *c); 

#ifdef DEBUG_LED
// Flashes the LED
// Count = number of times the LED is on
// delayPeriod = time in ms the LED is on 
void flash_led(uint8_t count, uint16_t delayPeriod);
#endif

//Programs the temporary buffer to a page in flash
void onboard_program_write(uint32_t page, uint8_t *buf);

// Handles a failed flash upload
// If flash has been partly written then it is erased and the bootloader re-runs
// Else boots the normal program
void UploadFailed();

//Reads UART until we have had at least 4 0xaa bytes then a ':'
//If other data arrives then start the normal app
void waitForStartBit();

#if defined(DEBUG_LED) || defined(DEBUG_STARTUP_DELAY)
// Blocks for the specified time
void delay_ms( int ms );
#endif

//Starts the program if exists else restarts bootloader
void app_start();
// Function pointer to the normal program
void (*flash_app_start)(void) = 0x0000;

//temporary storage for incoming UART data before being programmed to flash
uint8_t incoming_page_data[256];
// Length of incoming data
uint8_t page_length;

// Stores the last read byte and if its valid
uint8_t byteRead = 0xaa;
int8_t byteReadValid = 1;

// Indicates if a program memory page has been written yet
uint8_t pageProgrammAttempted = 0;

//Indicates if any program code has been found in flash (first 100 bytes)
uint8_t programExists = 0;

union page_address_union {
	uint16_t word;
	uint8_t  byte[2];
} page_address;


int main(void)
{
	// Bootloader address in datasheet pg 277 - Boot Size Configuration
	uint8_t check_sum = 0;
	uint16_t i;
	
	// 12-bit register which contains the USART baud rate
	// Setup USART baud rate
	UBRR0H = MYUBRR >> 8;	// four most significant
	UBRR0L = MYUBRR;		// eight least significant bits

	// Set the character size - 8bit
	UCSR0C = (1<<UCSZ00) | (1<<UCSZ01);

	// Asynchronous USART


	// Parity errors shown in UCSR0A - UPE0 (Set for error)
	// odd Parity
	//UCSR0C |=  (1<<UPM00) | (1<<UPM01);
	// even Parity
	UCSR0C |=  (1<<UPM01);

	// Frame error
	// UCSR0A - FE0 - (set for error)

	/* Enable internal pull-up resistor on pin D0 (RX), in order
	to suppress line noise that prevents the bootloader from
	timing out */
	DDRD &= ~_BV(PIND0);
	PORTD |= _BV(PIND0);

	// Enable Receiver and Transmitter
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);



	// Check if any program code has been loaded into flash
	{
		programExists = 0; // assume no program exists
		uint8_t b1 = 0;
		b1=pgm_read_byte(0); // Read the first byte of memory
		for(uint8_t x = 0; x < 100; x++){
			// Check if the first 100 bytes of memory are the same
			uint8_t readChar = pgm_read_byte(x);

#ifdef DEBUG_UART_OUT
		putch(readChar);
#endif
			// If byte different assume this means a program exists
			if (readChar != b1){
				programExists = 1;
			}
		}
	}



#ifdef DEBUG_IO
	// Set the pins as outputs
	DDRD = 0b11111111;
	PORTD |= 0b11110000;
	// Initilise all to high
	// Pins 3, 4 toggle twice on boot

	// Pin two toggle one if program memory is blank
	// pin two toggles twice if program exists

	// Pin 1 is toggled on successfully byte received
	// Pin 2 is toggled on full page received
	// Pin 3 goes low on app start called
	// Pin 4 goes low on first ":" received  

	// Toggle pins 3, 4 on successful upload
	// Toggle pin 2 for ever on upload fail

	PORTD &= 0b00001111;
	PORTD |= 0b11110000;

	if(programExists == 1){
		CLEAR_TESTPIN2;SET_TESTPIN2;
		CLEAR_TESTPIN2;SET_TESTPIN2;
	}else{
		CLEAR_TESTPIN2;SET_TESTPIN2;
	}

#endif

#ifdef DEBUG_LED
	//set LED pin as output
	LED_DDR |= _BV(LED);

	//flash onboard LED twice to signal entering of bootloader
	flash_led(2, 100);
#endif



#ifdef DEBUG_STARTUP_DELAY
	delay_ms(1000);
#endif

	// Flush any received junk data from UART
	while(UART_RECIVE_COMPLETE){
		UDR0;
	}

	//Start bootloading process

	// If bootloading the UART should read 0xaa at least 3 time until real data
	// If not bootloading then boot the main app
	byteRead = 0xaa;
	pageProgrammAttempted = 0;

	waitForStartBit();

	//First 2 bytes of data contain UART config
	uint8_t configUARTdata[2];

	byteReadValid = getch(&(configUARTdata[0]));
	if (byteReadValid == -1) app_start();
	byteReadValid = getch(&(configUARTdata[1]));
	if (byteReadValid == -1) app_start();

	 uint8_t parityBit = configUARTdata[0] >> 7;
	 uint16_t newBaudRate = (((uint16_t)configUARTdata[0] & 0b01111111) << 8) | (uint16_t)(configUARTdata[1]);
	 // Calculate the baud rate for the register
	 newBaudRate = (((((CPU_SPEED * 10) / (16L * newBaudRate)) + 5) / 10) - 1);
	 // 12-bit register which contains the USART baud rate
	 UBRR0H = newBaudRate >> 8;	// four most significant
	 UBRR0L = newBaudRate;		// eight least significant bits

	 // A parity bit is already set up for UART
	 // if we dont want the parity but then clear
	if (parityBit == 0)
		UCSR0C &=  !((1<<UPM00) | (1<<UPM01));

	// Flush any received junk data from UART
	while(UART_RECIVE_COMPLETE){
		UDR0;
	}

	// Wait for the start bit to indicate the program code is arriving
	byteRead = 0xaa;
	waitForStartBit();

#ifdef DEBUG_IO
	CLEAR_TESTPIN4;
#endif

	// already received ':' so skip
	goto SkipFirstChar;

	while(1)
	{

		// Get the next character
		// Looking for ':' The start of the data
		byteReadValid = getch(&byteRead);
		if (byteReadValid == -1 || byteRead != ':') UploadFailed();

SkipFirstChar:

		//The second character is the page length
		byteReadValid = getch(&page_length);
		if (byteReadValid == -1) UploadFailed();

		if (page_length == 'S') //Check to see if we are done - this is the "all done" command
		{
			boot_rww_enable (); //Wait for any flash writes to complete?

#ifdef DEBUG_IO
	//PORTD |= 0b11110000;
	PORTD &= 0b00001111;
	PORTD |= 0b11110000;
	//PORTD &= 0b00001111;
#endif

			flash_app_start();
		}

		//Get the memory address at which to store this block of data
		byteReadValid = getch(&(page_address.byte[0]));
		if (byteReadValid == -1) UploadFailed();
		byteReadValid = getch(&(page_address.byte[1]));
		if (byteReadValid == -1) UploadFailed();

		//Pick up the check sum for error detection
		byteReadValid = getch(&check_sum);
		if (byteReadValid == -1) UploadFailed();


		for(i = 0 ; i < page_length ; i++) //Read the program data
		{
			byteReadValid = getch(&(incoming_page_data[i]));
			if (byteReadValid == -1) UploadFailed();
		}
		
		//Calculate the checksum
		for(i = 0 ; i < page_length ; i++){
			check_sum = check_sum + incoming_page_data[i];
		}
		
		check_sum = check_sum + page_length;
		check_sum = check_sum + page_address.byte[0];
		check_sum = check_sum + page_address.byte[1];

#ifdef DEBUG_IO
		if (pinstate2 == 0){
			pinstate2 = 1;
			SET_TESTPIN2;
		}else{
			pinstate2 = 0;
			CLEAR_TESTPIN2;
		}
#endif

		if(check_sum == 0) //If we have a good transmission, put it in ink
			onboard_program_write((uint32_t)page_address.word, incoming_page_data);
		else // Error in the received page
			UploadFailed();
	}

}

//Reads UART until we have had at least 4 0xaa bytes then a ':'
//If other data arrives then start the normal app

void waitForStartBit(){
	int aaCount = 0;
	uint8_t hadLotsOfAAs = 0;

	while (byteRead == 0xaa){
		//Increment the received count
		aaCount++;
		if(aaCount > 4) hadLotsOfAAs = 1;
		//Get the next character
		byteReadValid = getch(&byteRead);
		// If not Data or errounious data then boot main app
		if (byteReadValid == -1){
			app_start();
		}
	}

	if(hadLotsOfAAs != 1|| byteRead != ':') app_start(); //Did not receive 3 0xaa or ':' the start of data char - start main app

}

void app_start(){
#ifdef DEBUG_IO
	CLEAR_TESTPIN3;
#endif
	// only run the program if program code has been found in the flash
	// else stay in the bootloader so programming can happen
	if (programExists == 1)
		flash_app_start();
	else
		main();
}

void UploadFailed(){

	// If the flash hasnt been touched yet, then boot the app 
	if (pageProgrammAttempted == 0 && programExists)
		app_start();

		// Disable interupts as required by LPM commands
	uint8_t sreg;
	sreg = SREG;
	cli();
	//Upload failed erase the first page of flash as it may contain corrupt data
	// This indicates on next foot there is no valid program
	for( uint32_t addr = 0; addr < APP_END; addr+=SPM_PAGESIZE){
		eeprom_busy_wait ();
		boot_spm_busy_wait ();
		boot_page_erase(addr);
		boot_spm_busy_wait();
		boot_rww_enable();
	}
	
	SREG = sreg;

	// Check is debug is active
#if defined(DEBUG_LED) || defined(DEBUG_IO)

	// WE are debugging - loop forever
	for(;;){

#ifdef DEBUG_LED
		flash_led(1,1000);
#endif

#ifdef DEBUG_IO
		if (pinstate2 == 0){
			pinstate2 = 1;
			SET_TESTPIN2;
		}else{
			pinstate2 = 0;
			CLEAR_TESTPIN2;
		}
#endif
	}
#endif

	//WE are not debugging
	//Start the bootloader again to try again
	main();

}


void onboard_program_write(uint32_t page, uint8_t *buf)
{
	uint16_t i;
	uint8_t sreg;

	// Disable interrupts.

	sreg = SREG;
	cli();

	eeprom_busy_wait ();

	boot_page_erase (page);
	boot_spm_busy_wait ();      // Wait until the memory is erased.

	for (i=0; i<SPM_PAGESIZE; i+=2)
	{
		// Set up little-endian word.

		uint16_t w = *buf++;
		w += (*buf++) << 8;

		boot_page_fill (page + i, w);
	}

	boot_page_write (page);     // Store buffer in flash page.
	boot_spm_busy_wait ();       // Wait until the memory is written.

	// Reenable RWW-section again. We need this if we want to jump back
	// to the application after bootloading.

	boot_rww_enable ();

	// Re-enable interrupts (if they were ever enabled).

	SREG = sreg;

	pageProgrammAttempted = 1;
}

#ifdef DEBUG_UART_OUT
void putch(char ch)
{
	// Check if the last data has been sent
	while (!(UCSR0A & _BV(UDRE0)));
	//Send next byte
	UDR0 = ch;
}
#endif

// Gets a char from the uart port
// Returns 1 upon success
// Return -1 upon failure
// Fails after timeout or upon parity error
int8_t getch(uint8_t *c)
{

	uint32_t count = 0;
	while(!(UART_RECIVE_COMPLETE))
	{
		count++;
		if (count > MAX_WAIT_IN_CYCLES) // Waited too long for data
		{
			return -1; // Timeout error
		}
	}

	//Check the data for any errors
	 if (UART_FRAME_ERROR || UART_PARITY_ERROR)
		return -1;

#ifdef DEBUG_IO
	if (pinstate == 0){
		pinstate = 1;
		SET_TESTPIN1;
	}else{
		pinstate = 0;
		CLEAR_TESTPIN1;
	}
#endif

	// Get the received byte out the received register
	(*c) = UDR0;

#ifdef DEBUG_UART_OUT
	//putch(*c);
#endif

	return 1;
}

#ifdef DEBUG_LED
void flash_led(uint8_t count, uint16_t delayPeriod)
{
	uint8_t i;

	for (i = 0; i < count; ++i) {
		LED_PORT |= _BV(LED); // LED on
		delay_ms(delayPeriod);
		LED_PORT &= ~_BV(LED); // LED off
		delay_ms(delayPeriod);
	}
}
#endif

#if defined(DEBUG_LED) || defined(DEBUG_STARTUP_DELAY)
void delay_ms( int ms )
{
	for (int i = 0; i < ms; i++)
	{
		_delay_ms(1); // Cant call with variable - therefore use this silly function
	}
}
#endif