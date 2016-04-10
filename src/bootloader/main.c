
#include "GlobalDefines.h"
#include "irboot/irboot.h"
#include "optboot/optiboot.h"


int main(void)
{

	/*
	*
	*	In order to program over usb the programmer will toggle DTR to reset the micro.
	*	If the micro is reset by the rest pin then the optiboot bootloader runs
	*	If no serial data is received the optiboot will watchdog timeout causing a reset
	*	
	*	If the reset pin wasnt toggled irboot runs (ie power on, software call or watchdog)
	*	If Invalid data is received then the flash program runs
	*
	*
	*/

	// Check if the micro was reset by the reset pin
	if (MCUSR & _BV(EXTRF))
		main_optiboot();

	main_irboot();

	// In case a bootloader returns by mistake
	for(;;){}

	return 1;
}
