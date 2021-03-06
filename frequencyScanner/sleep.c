/*
 * sleep.c
 *
 *  Created on: 02/12/2012
 *      Author: T
 */

#include <msp430.h>
#include "GenericTypeDefs.h"

volatile BOOL delayEnded=TRUE;

//Wait for the specified amount of microseconds
//Uses the TimerA0 CCR0, TimerA0 is sourced by SMCKL/8=1Mhz
void usleep(unsigned int useconds){
	//Disable interrupts while setting the registers
	__bic_SR_register(GIE);
	delayEnded=FALSE;
	//Set the TA0CCR0
	TA0CCR0=TA0R+useconds;
	//Enable capture compare interrupt
	TA0CCTL0 |= CCIE;
	//Re-enable interrupts
	__bis_SR_register(GIE);
	//Wait until interrupt vector changes global bool
	while(!delayEnded);

}

// Timer 0 CC0 interrupt service routine
__attribute__((interrupt(TIMER0_A0_VECTOR)))
void TIMER0_A0_ISR(void){
	//Clear the interrupt
	TA0CCTL0 &= ~CCIFG;
	//Set the global BOOL to TRUE
	delayEnded=TRUE;
	//Disable TimerA0 CC0 interrupts
	TA0CCTL0 &= ~CCIE;
}


