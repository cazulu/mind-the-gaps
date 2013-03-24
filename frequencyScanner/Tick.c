/*********************************************************************
 *
 *                  Tick Manager for MPS430
 *
 *********************************************************************
 *
 *
 * Author               Date        Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Nilesh Rajbharti     6/28/01     Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02      Cleanup
 * Nilesh Rajbharti     5/22/02     Rev 2.0 (See version.log for detail)
 * Howard Schlunder		6/13/07		Changed to use timer without 
 *									writing for perfect accuracy.
 * Javier Lara			9/05/12		Adapted code to MPS430
 ********************************************************************/
#define __TICK_C

#include "TCPIP.h"

// Internal counter to store Ticks.  This variable is incremented in an ISR and 
// therefore must be marked volatile to prevent the compiler optimizer from 
// reordering code to use this value in the main context while interrupts are 
// disabled.
static volatile DWORD dwInternalTicks = 0;

// 6-byte value to store Ticks.  Allows for use over longer periods of time.
static BYTE vTickReading[6];

static void GetTickCopy(void);

/*****************************************************************************
 Function:
 void TickInit(void)

 Summary:
 Initializes the Tick manager module.

 Description:
 Configures the Tick module and any necessary hardware resources.

 Precondition:
 None

 Parameters:
 None

 Returns:
 None

 Remarks:
 This function is called only once during lifetime of the application.
 ***************************************************************************/
void TickInit(void) {
	//Not needed, all this will be done in the function InitializeTimers() of config.c
}

/*****************************************************************************
 Function:
 static void GetTickCopy(void)

 Summary:
 Reads the tick value.

 Description:
 This function performs an interrupt-safe and synchronized read of the
 48-bit Tick value.

 Precondition:
 None

 Parameters:
 None

 Returns:
 None
 ***************************************************************************/
static void GetTickCopy(void) {
	TickUpdate();

	//We read the value of the TA1R 16bit timer register in a loop
	//The moment two consecutive reads yield the same, we consider that a sane value
	//This works if the MCLK freq >> Timer CLK(in this case, ACLK)
	unsigned short lastTA1R;
	do{
		lastTA1R = TA1R;
	}while(lastTA1R!=TA1R);

	//We copy the timer value into two least significant bytes of vTickReading
	vTickReading[0] = ((BYTE*) &lastTA1R)[0];
	vTickReading[1] = ((BYTE*) &lastTA1R)[1];
	*((DWORD*) &vTickReading[2]) = dwInternalTicks;
}

/*****************************************************************************
 Function:
 DWORD TickGet(void)

 Summary:
 Obtains the current Tick value.

 Description:
 This function retrieves the current Tick value, allowing timing and
 measurement code to be written in a non-blocking fashion.  This function
 retrieves the least significant 32 bits of the internal tick counter,
 and is useful for measuring time increments ranging from a few
 microseconds to a few hours.  Use TickGetDiv256 or TickGetDiv64K for
 longer periods of time.

 Precondition:
 None

 Parameters:
 None

 Returns:
 Lower 32 bits of the current Tick value.
 ***************************************************************************/
DWORD TickGet(void) {
	GetTickCopy();
	return *((DWORD*) &vTickReading[0]);
}

/*****************************************************************************
 Function:
 DWORD TickGetDiv256(void)

 Summary:
 Obtains the current Tick value divided by 256.

 Description:
 This function retrieves the current Tick value, allowing timing and
 measurement code to be written in a non-blocking fashion.  This function
 retrieves the middle 32 bits of the internal tick counter,
 and is useful for measuring time increments ranging from a few
 minutes to a few weeks.  Use TickGet for shorter periods or TickGetDiv64K
 for longer ones.

 Precondition:
 None

 Parameters:
 None

 Returns:
 Middle 32 bits of the current Tick value.
 ***************************************************************************/
DWORD TickGetDiv256(void) {
	DWORD dw;

	GetTickCopy();
	((BYTE*) &dw)[0] = vTickReading[1]; // Note: This copy must be done one
	((BYTE*) &dw)[1] = vTickReading[2]; // byte at a time to prevent misaligned
	((BYTE*) &dw)[2] = vTickReading[3]; // memory reads, which will reset the PIC.
	((BYTE*) &dw)[3] = vTickReading[4];

	return dw;
}

/*****************************************************************************
 Function:
 DWORD TickGetDiv64K(void)

 Summary:
 Obtains the current Tick value divided by 64K.

 Description:
 This function retrieves the current Tick value, allowing timing and
 measurement code to be written in a non-blocking fashion.  This function
 retrieves the most significant 32 bits of the internal tick counter,
 and is useful for measuring time increments ranging from a few
 days to a few years, or for absolute time measurements.  Use TickGet or
 TickGetDiv256 for shorter periods of time.

 Precondition:
 None

 Parameters:
 None

 Returns:
 Upper 32 bits of the current Tick value.
 ***************************************************************************/
DWORD TickGetDiv64K(void) {
	DWORD dw;

	GetTickCopy();
	((BYTE*) &dw)[0] = vTickReading[2]; // Note: This copy must be done one
	((BYTE*) &dw)[1] = vTickReading[3]; // byte at a time to prevent misaligned
	((BYTE*) &dw)[2] = vTickReading[4]; // memory reads, which will reset the PIC.
	((BYTE*) &dw)[3] = vTickReading[5];

	return dw;
}

/*****************************************************************************
 Function:
 DWORD TickConvertToMilliseconds(DWORD dwTickValue)

 Summary:
 Converts a Tick value or difference to milliseconds.

 Description:
 This function converts a Tick value or difference to milliseconds.  For
 example, TickConvertToMilliseconds(32768) returns 1000 when a 32.768kHz
 clock with no prescaler drives the Tick module interrupt.

 Precondition:
 None

 Parameters:
 dwTickValue	- Value to convert to milliseconds

 Returns:
 Input value expressed in milliseconds.

 Remarks:
 This function performs division on DWORDs, which is slow.  Avoid using
 it unless you absolutely must (such as displaying data to a user).  For
 timeout comparisons, compare the current value to a multiple or fraction
 of TICK_SECOND, which will be calculated only once at compile time.
 ***************************************************************************/
DWORD TickConvertToMilliseconds(DWORD dwTickValue) {
	return (dwTickValue + (TICKS_PER_SECOND / 2000ul))
			/ ((DWORD) (TICKS_PER_SECOND / 1000ul));
}

/*****************************************************************************
 Function:
 void TickUpdate(void)

 Description:
 Checks if there's been an overflow in Timer A and
 updates the tick value accordingly. It should be called
 at least once every 2 seconds(period of 32KHz ACLK)
 to maintain an accurate timer

 Precondition:
 None

 Parameters:
 None

 Returns:
 None
 ***************************************************************************/
void TickUpdate(void) {
	if(TA1CTL & TAIFG){
		TA1CTL &= ~TAIFG;
		// Increment internal high tick counter
		dwInternalTicks++;
	}
}
