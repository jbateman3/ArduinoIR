#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#define PAGE_SIZE 128
#define MEMORY_SIZE 30720


UINT8 intel_hex_array[MEMORY_SIZE];
 
char * defaultComPort = "COM5";
char * defaultHexFile = "input.hex";

int baudRate = 4800;
char * comPort = defaultComPort;
char * hexPath = defaultHexFile;
int IrPeriod = 26;
int verboseMode = 0;
int quiteMode = 0;
int automaticReset = 0;
int useParityBit = 0;
int USBMode = 0;


// Declare variables and structures for Serial
HANDLE hSerial;
DCB dcbSerialParams = { 0 };
COMMTIMEOUTS timeouts = { 0 };

int setBaudRate(int baudRate, int parity);
int ReadByte();


void printHelp() {
	printf("\n");
	printf("Atmega328 OTA IT Programming\n");
	printf("\n");
	printf("-P        Enables an Even parity bit\n");
	printf("-b        Baud Rate\n");
	printf("-p        Com Port\n");
	printf("-i        IR Carrior Period\n");
	printf("-f        Input Hex file\n");
	printf("-a        Automatic Reset\n");
	printf("-u        USB mode - no IR\n");
	printf("-v        Verbose - Output more info\n");
	printf("-q        Quite - Supress fancy progress bar\n");
	printf("-h        This help\n");
	printf("\n");
	printf("Example:\n");
	printf("AvrDudeIR -b 4800 -p COM5 -f Sketch.hex\n");
}

int OpenSerialPort() {

	char ComPortName[50];
	sprintf(ComPortName, "\\\\.\\%s", comPort);
	wchar_t wComPortName[20];
	mbstowcs(wComPortName, ComPortName, strlen(ComPortName) + 1);

	// Open the highest available serial port number
	printf("Opening serial port...");
	hSerial = CreateFile(
		wComPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hSerial == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Error\n");
		return 0;
	}
	else fprintf(stderr, "OK\n");


	if (setBaudRate(4800, 0) == 0)
		return 0;

	// Set COM port timeout settings
	timeouts.ReadIntervalTimeout = 1000;
	timeouts.ReadTotalTimeoutConstant = 1000;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 5000;
	timeouts.WriteTotalTimeoutMultiplier = 1000;
	if (SetCommTimeouts(hSerial, &timeouts) == 0)
	{
		fprintf(stderr, "Error setting timeouts\n");
		CloseHandle(hSerial);
		return 0;
	}

	if (SetCommMask(hSerial, EV_TXEMPTY) == 0) {
		fprintf(stderr, "Error setting TX Events\n");
		CloseHandle(hSerial);
		return 0;
	}

	return 1;

}

int closeSerialPort() {
	fprintf(stderr, "Closing serial port...");
	if (CloseHandle(hSerial) == 0)
	{
		fprintf(stderr, "Error\n");
		return 1;
	}
	fprintf(stderr, "OK\n");

}

int setBaudRate(int baudRate, int parity) {
	//DWORD eventType = 0;
	//if (WaitCommEvent(hSerial, &eventType, NULL) == 0) {
	//	printf("Failed to wait for TX Event\n");
	//	printf("Wait failed with error %d.\n", GetLastError());
	//	CloseHandle(hSerial);
	//	return 0;
	//}
	if (PurgeComm(hSerial, PURGE_TXCLEAR) == 0) {
		printf("Failed to clear TX buffer\n");
		CloseHandle(hSerial);
		return 0;
	}
	// Set device parameters (38400 baud, 1 start bit,
	// 1 stop bit, no parity)
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	if (GetCommState(hSerial, &dcbSerialParams) == 0)
	{
		fprintf(stderr, "Error getting device state\n");
		CloseHandle(hSerial);
		return 0;
	}

	dcbSerialParams.BaudRate = baudRate;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	if (parity == 1)
		dcbSerialParams.Parity = PARITY_EVEN;
	else
		dcbSerialParams.Parity = PARITY_NONE;

	if (SetCommState(hSerial, &dcbSerialParams) == 0)
	{
		fprintf(stderr, "Error setting device parameters\n");
		CloseHandle(hSerial);
		return 0;
	}

	return 1;
}

int SendByte(UINT8 b) {

	char bytes_to_send[1];
	bytes_to_send[0] = b;

	// Send specified text (remaining command line arguments)
	DWORD bytes_written, total_bytes_written = 0;

	if (!WriteFile(hSerial, bytes_to_send, 1, &bytes_written, NULL))
	{
		fprintf(stderr, "Error sending Byte\n");
		CloseHandle(hSerial);
		return 0;
	}

	if (bytes_written != 1)
		printf("Error Sendinf Bytes!\n");

	return 1;
	
}

int bytesSent = 0;

int SendByteLimit(UINT8 b) {
	if (USBMode == 0 && bytesSent > 100) {
		if (ReadByte() == 0) {
			printf("Programmer not responding, didnt request more data\n");
			return 0;
		}
		bytesSent = 0;
	}
	SendByte(b);
	bytesSent++;

}

int ReadByte() {
	char readByte;
	DWORD bytesRead = 0;
	if (!ReadFile(hSerial, &readByte, 1, &bytesRead, NULL)) {
		fprintf(stderr, "Error reading Byte\n");
		CloseHandle(hSerial);
		return -1;
	}
	if (bytesRead == 1)
		return readByte;
	else
		return -1;
}

int SendUSBHeadder() {

	if (EscapeCommFunction(hSerial, SETDTR) == 0)
		printf("Error Setting DTR\n");

	if (EscapeCommFunction(hSerial, CLRDTR) == 0)
		printf("Error Clearing DTR\n");

	// Keep the micro in bootloader while resetting
	for (int x = 0; x < 20; x++) {
		SendByte(0xaa);
	}

	// Both Setup Bytes euqal 1 for USB mode
	SendByte(0x01);
	SendByte(0x01);

	Sleep(50);

	return 1;
}

int SendHeadder() {
	UINT8 setupdata[3];

	setupdata[0] = IrPeriod;

	if (useParityBit == 1)
		setupdata[1] = (baudRate / 256) + 128;
	else
		setupdata[1] = (baudRate / 256);

	setupdata[2] = baudRate % 256;

	int sucess = 1;

	for (int retryCount = 0; retryCount < 3; retryCount++) {

		sucess = 1;

		SendByte(setupdata[0]);
		SendByte(setupdata[1]);
		SendByte(setupdata[2]);

		printf("Waiting for programmer response...");

		UINT8 irByte = ReadByte();
		UINT8 baudRateHigh = ReadByte();
		UINT8 baudRateLow = ReadByte();
		UINT8 parity = ReadByte();
		int baudRateResponse = baudRateHigh * 256 + baudRateLow;

		if (IrPeriod != irByte || baudRate != baudRateResponse || parity != useParityBit) {
			printf("Invalid Programmer response....\n");
			sucess = 0;
		}

		if (sucess)
			break;
	}

	// Set the UART setting to the negaushates settings
	setBaudRate(baudRate, useParityBit);

	return sucess;

}

int HexConvert(char b2, char b1) {
	int val = 0;
	int tmp = 0;

	if (b1 >= 'A' && b1 <= 'F')
		tmp = b1 - 'A' + 10;
	else if (b1 >= '0' && b1 <= '9')
		tmp = b1 -'0';

	val = val * 16 + tmp;

	if (b2 >= 'A' && b2 <= 'F')
		tmp = b2 - 'A' + 10;
	else if (b2 >= '0' && b2 <= '9')
		tmp = b2 - '0';

	val = val * 16 + tmp;
	return val;
}

int parseHexFile() {

	for (int x = 0; x < MEMORY_SIZE; x++)
		intel_hex_array[x] = 255;

	FILE * f1 = fopen(hexPath, "r");

	if (f1 == NULL)
		return 0;

	int readChar = 0;

	// Find the begning of a usefull line
	while(readChar != ':')
		readChar = fgetc(f1);

	while (1) {
		int recordLength = HexConvert(fgetc(f1), fgetc(f1));
		int Memory_Address_High = HexConvert(fgetc(f1), fgetc(f1));
		int Memory_Address_Low = HexConvert(fgetc(f1), fgetc(f1));

		int MemoryAddress = (Memory_Address_High * 256) + Memory_Address_Low;

		int LastLine = HexConvert(fgetc(f1), fgetc(f1));
		if (LastLine == 1)
			break;

		for (int x = 0; x < recordLength; x++)
			intel_hex_array[MemoryAddress + x] = HexConvert(fgetc(f1), fgetc(f1));

		int checkSum = HexConvert(fgetc(f1), fgetc(f1));

		readChar = 0;
		while (readChar != ':')
			readChar = fgetc(f1);

	}
	fclose(f1);
	return 1;


}


int main(int argc, char **argv)
{

	// No arguments passed to the program
	if (argc < 2)
		printHelp();

	// Read in the passed arguments
	for (int x = 0; x < argc; x++)
	{
		if (strcmp("-b", argv[x]) == 0) {
			baudRate = atoi(argv[x + 1]);
			x++;
		}
		else if (strcmp("-p", argv[x]) == 0) {
			comPort = argv[x + 1];
			x++;
		}
		else if (strcmp("-i", argv[x]) == 0) {
			IrPeriod = atoi(argv[x + 1]);
			x++;
		}
		else if (strcmp("-f", argv[x]) == 0) {
			hexPath = argv[x + 1];
			x++;
		}
		else if (strcmp("-h", argv[x]) == 0) {
			printHelp();
		}
		else if (strcmp("-v", argv[x]) == 0) {
			verboseMode = 1;
		}
		else if (strcmp("-q", argv[x]) == 0) {
			quiteMode = 1;
		}
		else if (strcmp("-a", argv[x]) == 0) {
			automaticReset = 1;
		}
		else if (strcmp("-P", argv[x]) == 0) {
			useParityBit = 1;
		}
		else if (strcmp("-u", argv[x]) == 0) {
			USBMode = 1;
		}
		else {
			printf("Invalid argument passed:\n %s, \n", argv[x]);
		}
	}

	if (USBMode == 1)
		printf("Programming using IR!\n");
	else
		printf("Programming using USB!");

	if (verboseMode == 1) {
		printf("\n");
		printf("COM Port: %s\n", comPort);
		printf("Hex File: %s\n", hexPath);
		printf("IR Period: %dus\n", IrPeriod);
		printf("IR Frequency: %fkHz\n", 1.0 / ((float)IrPeriod * 0.001));
		printf("Baud Rate: %d\n", baudRate);
		printf("Parity Bit: %d\n", useParityBit);
		printf("Automatic Reset: %d\n", automaticReset);
		printf("USB Mode: %d\n", USBMode);
		printf("Quiet Mode: %d\n", quiteMode);
		printf("Verbose Mode: %d\n", verboseMode);
		printf("\n");
	}

	printf("Reading Hex file....");

	if (parseHexFile() == 1)
		printf("Done\n");
	else
		printf("Failed\n");

	printf("Opening Serial port....");

	if (OpenSerialPort() == 1)
		printf("Done\n");
	else
		printf("Failed\n");

	if (baudRate > 32767)
		printf("Baud rate is too high, must be less than 32700\n");

	if (USBMode == 1) {
		printf("USB Mode, overidding baud rate to 38400\n");
		baudRate = 38400;
	}


	if (USBMode)
		SendUSBHeadder();
	else
		SendHeadder();


	if (USBMode == 1) {
		setBaudRate(BAUD_9600, 1);
		// Keep the micro in bootloader after chaning baud settings
		for (int x = 0; x < 20; x++) {
			SendByte(0xaa);
		}
		printf("Hopefully automaticly reset, starting programming\n");
	}
	else if (automaticReset == 1) {
		printf("Hopefully automaticly reset, starting programming\n");
	}
	else {
		printf("Reset the target and press OK to begin programing\n");
		MessageBox(NULL, L"Reset the target and press OK to begin programing", L"Ready to program", MB_OK);
	}


	if (USBMode == 0)
		SendByte('s');

	int Memory_Address_High;
	int Memory_Address_Low;

	int Check_Sum;

	int  current_memory_address;
	int  last_memory_address;

	last_memory_address = MEMORY_SIZE - 1;
	while (last_memory_address > 0) {
		if (intel_hex_array[last_memory_address] != 255)
			break;
		last_memory_address = last_memory_address - 1;
	}

	printf("Programming...\n");

	clock_t beginTime, endTime;
	beginTime = clock();

	current_memory_address = 0;

	while (current_memory_address < last_memory_address) {

		printf("Writing : ");
		float percent = ((float)current_memory_address / (float)last_memory_address) * 100;
		for (int x = 0; x < 50; x++) {
			if (x * 2 < percent)
				printf("#");
			else
				printf(" ");
		}
		clock_t nowTime = clock();

		printf(" : %.2f%%     %.2fs\n", percent, ((float)nowTime - (float)beginTime) / CLOCKS_PER_SEC);

		Memory_Address_High = current_memory_address / 256;
		Memory_Address_Low = current_memory_address % 256;

		Check_Sum = 0;
		Check_Sum += PAGE_SIZE;
		Check_Sum += Memory_Address_High;
		Check_Sum += Memory_Address_Low;

		for (int x = 0; x < PAGE_SIZE; x++)
			Check_Sum += intel_hex_array[current_memory_address + x];
		
		while (Check_Sum > 256)
			Check_Sum -= 256;

		Check_Sum = 256 - Check_Sum;

		SendByteLimit(':');

		SendByteLimit(PAGE_SIZE);

		SendByteLimit(Memory_Address_Low);
		SendByteLimit(Memory_Address_High);

		SendByteLimit(Check_Sum);

		for (int x = 0; x < PAGE_SIZE; x++)
			SendByteLimit(intel_hex_array[current_memory_address + x]);

		if (USBMode == 1) {
			if (ReadByte() == -1) {
				printf("Micro never requested next page, programming failed\n");
				return 0;
			}
		}
		current_memory_address = current_memory_address + PAGE_SIZE;
	}

	SendByteLimit(':');
	SendByteLimit('S');

	endTime = clock();

	printf("Writing : ################################################## : 100%     %ss\n", (endTime - beginTime) / CLOCKS_PER_SEC);

	printf("Done uploading, lets hope it worked...\n\n");

	// Close serial port
	closeSerialPort();

	// exit normally
	return 1;
}