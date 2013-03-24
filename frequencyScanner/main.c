//***************************************************************************************
//  MSP430 CC1101 ENC28J60 Tutorial
//
//  Albert Lopez and Francisco Sanchez
//  Feb 2012
//  Built with Code Composer Studio v4
//

#include <msp430.h>
#include <string.h>
#include "GenericTypeDefs.h"
#include "hardware_board.h"
#include "config.h"
#include "MAC.h"
#include "StackTsk.h"
#include "Tick.h"
#include "appconfig.h"
#include "gw.h"

#define APP_STANDBY_MODE	0   
#define	APP_NORMAL_MODE		1
#define ENC28J60_REV		6
#define CC1101_REV			4



int main(void) {
	WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer

	/*** PERIPHERALS INITIALIZATION ***/
	InitializeClocks(); // Initialize Unified Clock System
	InitializeTimers(); // Initialize Timers

	/*** BOARD INITIALIZATION ***/
//	InitializeButton(); // Initialize Button
	InitializeLeds(); // Initialize LEDs
	InitializeIO();

	/**** ETHERNET INITIALIZATION ****/
	InitAppConfig();
	InitializeEthSpi(); // Configure SPI module for ENC28J60
	StackInit(); // Initialize Stack

	//__bis_SR_register(LPM3_bits + GIE);

	LED_OUT &= ~(LED1+LED2);
	LED_OUT |= LED1;

	//Wait until the stack is properly configured to enter the main program
	while (StackIsInConfigMode()) {
		StackTask();
	}

	LED_OUT &= ~LED1;
	LED_OUT |= LED2;

	GWInit();

	DWORD ledTimer=TickGet();
	DWORD currentTimer=TickGet();
	while (1) {
		currentTimer=TickGet();
		if(currentTimer-ledTimer>TICK_SECOND){
			LED_OUT ^= LED1 + LED2;
			ledTimer=TickGet();
		}
		StackTask();
		GWTask();
	}
}

