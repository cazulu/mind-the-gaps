#include <msp430.h>
#include <stdlib.h>
#include "hal_pmm.h"
#include "flash.h"
#include "GenericTypeDefs.h"
// LEDs
#define LED1		BIT4
#define LED2		BIT5
#define LED_OUT	    P6OUT
#define TIME_BLINK	16384
// BUTTON
#define BUTTON		BIT5
#define BUTTON_IN   P1IN
#define BUTTON_OUT  P1OUT
#define BUTTON_DIR  P1DIR
#define BUTTON_REN  P1REN
#define BUTTON_IES  P1IES
#define BUTTON_IE   P1IE
#define BUTTON_IFG  P1IFG

static BOOL bootloaderRequested = FALSE;

void InitializeClocks();
void InitializeTimers();
void InitializeButton();
void switchToBootloader();

int main(void) {
	WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer
	P6DIR |= LED1 + LED2; // Set P6.4 and P6.5 to output direction

	LED_OUT &= ~LED1;
	LED_OUT |= LED2;
	bootloaderRequested = FALSE;

	InitializeButton();
	InitializeClocks();
	InitializeTimers();

	for (;;) {
		volatile unsigned int i; // volatile to prevent optimization
		LED_OUT ^= LED1 + LED2;
		if (bootloaderRequested){
			//Disable the interruptions, we don't want a reset
			//to arrive while writing the reset vector
			__bic_SR_register(GIE);
			switchToBootloader();
		}
		__bis_SR_register(LPM3_bits + GIE);
	}
}

//Changes the reset vector to the bootloader start position
//and triggers a software POR
//IMPORTANT: Change the resetVectorLocation
//and the bootloaderReset values to suit the current code
void switchToBootloader() {
//	char resetVectorLocation[] = "FFFE";
	BYTE bootloaderResetValue[2] = { 0x82, 0x5D };
	unsigned int i = 6;
	BYTE * resetCodePtr = (BYTE*)0xFFFE;
	//Pointer to the start of the last memory segment of flash bank 0
	BYTE * tempCodePtr = resetCodePtr - 510;
	//Array to save the values of the last segment of flash bank 0
	//because it will have to be erased to change the reset vector
	BYTE * segmentBackup = (BYTE *) malloc(sizeof(BYTE)*512);

	//malloc returns NULL if it fails, and in
	//that case we exit the function
	if (segmentBackup == NULL)
		return;

	//Backup the memory segment before erasing it
	for(i=0; i<510; i++){
		segmentBackup[i]=*tempCodePtr++;
	}

	//Erase the flash segment
	Flash_segmentErase(__MSP430_BASEADDRESS_FLASH__, resetCodePtr);
	tempCodePtr = resetCodePtr - 510;
	//Restore the backup values
	Flash_write8(__MSP430_BASEADDRESS_FLASH__, segmentBackup, tempCodePtr, 510);
	//Write the reset vector
	Flash_write8(__MSP430_BASEADDRESS_FLASH__, bootloaderResetValue,
				resetCodePtr, 2);

	//Be nice and free the memory
	free(segmentBackup);

	//Trigger software POR
	PMMCTL0 |= PMMSWPOR;

}

// ACLK = XT1CLK = XT1LF (32kHz)
// MCLK = DCODIV (8MHz)
// SMCLK = DCODIV (8MHz)
void InitializeClocks() {
	SetVCore(PMMCOREV_1); // Set VCore = 1.6V for 8MHz clock
	P7SEL |= BIT0 + BIT1; // Select XT1
	P5SEL |= BIT2 + BIT3; // Select XT2
	UCSCTL6 &= ~(XT1OFF + XT2OFF); // XT1 On, XT2 On
	UCSCTL6 |= XCAP_3; // Internal load cap

	__bis_SR_register(SCG0);
	// Disable the FLL control loop
	UCSCTL0 = 0x0000; // Set DCOx = 31, MODx = 0
	UCSCTL1 = DCORSEL_7; // Select DCO range 16MHz operation
	UCSCTL2 = FLLD_1 + 243; // Set DCO Multiplier for 12MHz
							// (N + 1) * FLLRef = Fdco
							// (243 + 1) * 32768 = 8MHz
							// Set FLL Div = fDCOCLK/2
	__bic_SR_register(SCG0);
	// Enable the FLL control loop

// Worst-case settling time for the DCO when the DCO range bits have been
// changed is n x 32 x 32 x f_MCLK / f_FLL_reference. See UCS chapter in 5xx
// UG for optimization.
// 32 x 32 x 12 MHz / 32,768 Hz = 375000 = MCLK cycles for DCO to settle
	__delay_cycles(250000);

// Loop until XT1,XT2 & DCO fault flag is cleared
	do {
		UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
		// Clear XT2,XT1,DCO fault flags
		SFRIFG1 &= ~OFIFG; // Clear fault flags
	} while (SFRIFG1 & OFIFG); // Test oscillator fault flag

	UCSCTL6 &= ~XT1DRIVE_3; // Xtal is now stable, reduce drive strength
}

void InitializeTimers() {
	TA1CTL |= TASSEL_1 + MC_2 + TACLR; // TACLK = ACLK, Continuous mode.

	TA1CCR1 = TIME_BLINK; // TACLK = ACLK, Continuous mode.
	TA1CCTL1 |= CCIE; // TA1CCTL1 Capture Compare
	TA1CCTL1 &= ~CCIFG;
}

void InitializeButton(void) // Configure Push Button
{
	BUTTON_DIR &= ~BUTTON; // In
	BUTTON_REN |= BUTTON; // Pull-up/down enabled
	BUTTON_OUT |= BUTTON; // Pull-up
	BUTTON_IES |= BUTTON; // High to Low
	BUTTON_IFG &= ~BUTTON;
	BUTTON_IE |= BUTTON;
}

//Interrupt vector for Timer 1 Overflow
#pragma vector=TIMER1_A1_VECTOR
__interrupt void TIMER1_A1_ISR(void) {
	TA1CCR1 += TIME_BLINK;
	TA1CCTL1 &= ~CCIFG;
	__bic_SR_register_on_exit(LPM3_bits);
}

// Port1 Interrupt for Button press
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void) {
	// Button Interrupt
	if (BUTTON_IFG & BUTTON) {
		BUTTON_IFG &= ~BUTTON;
		BUTTON_IE &= ~BUTTON; // Debounce
		WDTCTL = WDT_ADLY_250; // 250ms assuming 32768Hz for ACLK
		SFRIFG1 &= ~WDTIFG; // clear interrupt flag
		SFRIE1 |= WDTIE;
		bootloaderRequested = TRUE;
	}
}

// WDT Interrupt Service Routine used to de-bounce button press
#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR(void) {
	SFRIE1 &= ~WDTIE; // disable interrupt
	SFRIFG1 &= ~WDTIFG; // clear interrupt flag
	WDTCTL = WDTPW + WDTHOLD; // put WDT back in hold state
	BUTTON_IE |= BUTTON; // Debouncing complete
}
