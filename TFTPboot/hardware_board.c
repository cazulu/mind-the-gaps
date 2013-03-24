#include "hardware_board.h"
#include <msp430.h>


void InitializeButton(void)     	// Configure Push Button 
{
  BUTTON_DIR &= ~BUTTON;			// In
  BUTTON_REN |= BUTTON;				// Pull-up/down enabled	
  BUTTON_OUT |= BUTTON;				// Pull-up
  BUTTON_IES |= BUTTON;				// High to Low
  BUTTON_IFG &= ~BUTTON;
  BUTTON_IE |= BUTTON;
}

void InitializeLeds(void)			// Configure LEDs
{
  LED_DIR |= LED1 + LED2;                          
  LED_OUT &= ~(LED1 + LED2); 
}


void InitializeIO(void)			// Configure I/O
{

	ETH_RST_DIR |= ETH_RST;			// Out
	ETH_RST_OUT |= ETH_RST;

	ETH_INT_DIR &= ~ETH_INT;		// In
	ETH_INT_OUT |= ETH_INT;
	ETH_INT_REN |= ETH_INT;			// Pull-up enabled		
	ETH_INT_IES |= ETH_INT;			// High to Low
	ETH_INT_IFG &= ~ETH_INT;
	ETH_INT_IE |= ETH_INT;	
	
}

