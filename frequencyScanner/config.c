#include "config.h"
#include "hardware_board.h"
#include "hal_pmm.h"
#include <msp430.h>

// ENC28J60 SPI port
#define ETH_SIMO		BIT1
#define ETH_SOMI		BIT2
#define ETH_SCLK		BIT3
#define ETH_SPI_IN		P3IN
#define ETH_SPI_OUT		P3OUT
#define ETH_SPI_DIR		P3DIR
#define ETH_SPI_REN		P3REN
#define ETH_SPI_SEL		P3SEL


// RF SPI port
#define RF_SIMO			BIT6
#define RF_SOMI			BIT7
#define RF_SPI_IN		P5IN
#define RF_SPI_OUT		P5OUT
#define RF_SPI_DIR		P5DIR
#define RF_SPI_REN		P5REN
#define RF_SPI_SEL		P5SEL

#define RF_SCLK			BIT6
#define RF_SPI_SCLK_DIR	P3DIR
#define RF_SPI_SCLK_SEL	P3SEL



	// ACLK = XT1CLK = XT1LF (32kHz)
	// MCLK = DCODIV (8MHz)
	// SMCLK = DCODIV (8MHz)
void InitializeClocks(void)
{
	  SetVCore(PMMCOREV_1);                     // Set VCore = 1.6V for 8MHz clock
  P7SEL |= BIT0 + BIT1;                     // Select XT1
  P5SEL |= BIT2 + BIT3;						// Select XT2
  UCSCTL6 &= ~(XT1OFF + XT2OFF);            // XT1 On, XT2 On
  UCSCTL6 |= XCAP_3;                        // Internal load cap
  
  __bis_SR_register(SCG0);                  // Disable the FLL control loop
  UCSCTL0 = 0x0000;  						// Set DCOx = 31, MODx = 0
  UCSCTL1 = DCORSEL_7;                      // Select DCO range 16MHz operation
  UCSCTL2 = FLLD_1 + 243;                   // Set DCO Multiplier for 12MHz
                                            // (N + 1) * FLLRef = Fdco
                                            // (243 + 1) * 32768 = 8MHz
                                            // Set FLL Div = fDCOCLK/2
  __bic_SR_register(SCG0);                  // Enable the FLL control loop

  // Worst-case settling time for the DCO when the DCO range bits have been
  // changed is n x 32 x 32 x f_MCLK / f_FLL_reference. See UCS chapter in 5xx
  // UG for optimization.
  // 32 x 32 x 12 MHz / 32,768 Hz = 375000 = MCLK cycles for DCO to settle
  __delay_cycles(125000);
  __delay_cycles(125000);
	
  // Loop until XT1,XT2 & DCO fault flag is cleared
  do
  {
    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
                                            // Clear XT2,XT1,DCO fault flags
    SFRIFG1 &= ~OFIFG;                      // Clear fault flags
  }while (SFRIFG1&OFIFG);                   // Test oscillator fault flag
  
  UCSCTL6 &= ~XT1DRIVE_3;         	// Xtal is now stable, reduce drive strength 
}


void InitializeTimers(void)
{
	//TimerA0=SMCLK/8=1Mhz, mode continuous up, used to time microsecond events
	TA0CTL |= TASSEL__SMCLK + MC__CONTINOUS + TACLR + ID__8;

	TA1CTL |= TASSEL_1 + MC_2 + TACLR;                  // TACLK = ACLK, Continuous mode.

//   TA1CCR1 = TIME_STACK;            // TACLK = ACLK, Continuous mode.
//   TA1CCTL1 |= CCIE;                // TA1CCTL1 Capture Compare
//   TA1CCTL1 &= ~CCIFG;

//   TA1CCR2 = TIME_BLINK;                         // TACLK = ACLK, Continuous mode.
//   TA1CCTL2 |= CCIE;                // TA1CCTL2 Capture Compare
//   TA1CCTL2 &= ~CCIFG;
}

void InitializeEthSpi(void) 
{
	// Activate reset state
	UCB0CTL1 |= UCSWRST;
	
	// Configure ports		
	ETH_SPI_SEL |= ETH_SCLK + ETH_SIMO + ETH_SOMI;	// Special functions for SPI pins	
	ETH_SPI_DIR |= ETH_SIMO + ETH_SCLK;	// Outputs

	ETH_CS_DIR |= ETH_CS;
	ETH_CS_OUT |= ETH_CS;
	
	// Configure SPI registers
	UCB0CTL0 |= UCCKPH + UCMSB + UCMST + UCSYNC;	// Clock phase 0, Clock pol 0, 8-bit
										// MSB first, Master mode, 3-pin SPI, Synch
	UCB0CTL1 |= UCSSEL_2;				// SMCLK clock source
	UCB0BR0 = 0;						// No Prescaler (8MHz)
	UCB0BR1 = 0;
	UCA0MCTL = 0;										
	
	// Deactivate reset state		
	UCB0CTL1 &= ~UCSWRST;
}

