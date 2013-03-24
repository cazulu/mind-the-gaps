#ifndef HARDWARE_BOARD_H_
#define HARDWARE_BOARD_H_


// LEDs
#define LED1		BIT4
#define LED2		BIT5
#define LED_OUT	    P6OUT
#define LED_DIR 	P6DIR

// BUTTON
#define BUTTON		BIT5
#define BUTTON_IN   P1IN
#define BUTTON_OUT  P1OUT
#define BUTTON_DIR  P1DIR
#define BUTTON_REN  P1REN
#define BUTTON_IES  P1IES
#define BUTTON_IE   P1IE
#define BUTTON_IFG  P1IFG

// ENC28J60
#define ETH_CS			BIT0
#define ETH_CS_IN		P3IN
#define ETH_CS_OUT		P3OUT
#define ETH_CS_DIR		P3DIR
#define ETH_CS_REN		P3REN

#define ETH_INT			BIT2
#define ETH_INT_IN		P1IN
#define ETH_INT_DIR		P1DIR
#define ETH_INT_OUT		P1OUT
#define ETH_INT_REN		P1REN
#define ETH_INT_IES 	P1IES
#define ETH_INT_IE  	P1IE
#define ETH_INT_IFG 	P1IFG

#define ETH_RST			BIT3
#define ETH_RST_OUT		P1OUT
#define ETH_RST_DIR		P1DIR



void InitializeButton(void);
void InitializeLeds(void);
void InitializeIO(void);


#endif /*HARDWARE_BOARD_H_*/
