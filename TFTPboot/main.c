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
#include "Parser.h"

#define APP_STANDBY_MODE	0   
#define	APP_NORMAL_MODE		1
#define ENC28J60_REV	6
#define TIMEOUT_RX		300

static char applicationMode = APP_STANDBY_MODE;

// ***TFTP server variable definitions here***
//Stores the amount of bytes received from the last data block
static int bytesReceived;
static TFTP_RESULT clientStatus = TFTP_NOT_READY;
static BOOL isFileRx = FALSE;
volatile BOOL isTFTPActivated = FALSE;
static BOOL blockRead;
static BYTE tftpTxFileName[7];

//Variable to store the status of the parser
static PARSER_STATUS parserStatus;

//Pointer to a buffer for a TFTP data block
//It is allocated in TFTPInit()
static BYTE * tftpBuffer;
//A TFTP data block has a maximum of 512B
#define TFTP_BUFFER_SIZE 512

static BYTE i;
static BYTE retriesLeft;
static BYTE lastTxByte;
static BYTE currentTxByte;
static DWORD codeSize;
void TFTPInit();
void TFTPTask();
static BOOL TFTPWrite();
static BOOL TFTPRead();

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
	//Wait until the stack and the TFTP application are properly configured to enter the main program
	while (StackIsInConfigMode() && TFTPIsInConfigMode()) {
		StackTask();
	}
	LED_OUT &= ~LED1;
	LED_OUT |= LED2;

	//Initialize the TFTP variables
	//Make sure that you get the TFTP server address from Appconfig
	//once DHCP has finished, that is to say, the stack is not in config mode
	TFTPInit();

	isTFTPActivated = TRUE;

	//Main program loop -> TFTP echo client: Requests a file and then sends it back
	//NOTE: There is absolutely no handling of error conditions
	while (1) {
		StackTask();
		StackApplications();
		TFTPTask();

		//If the bootfile is correctly received
		//we trigger a software POR(Power On Reset)
		//to switch to the user application
		if(parserStatus==PARSER_END_OF_CODE)
			PMMCTL0 |= PMMSWPOR;
	}
}

//Initialize TFTP variables
void TFTPInit() {
	bytesReceived = 0;
	blockRead = FALSE;
	//TxFileName: lol.txt
	tftpTxFileName[0] = 'l';
	tftpTxFileName[1] = 'o';
	tftpTxFileName[2] = 'l';
	tftpTxFileName[3] = '.';
	tftpTxFileName[4] = 't';
	tftpTxFileName[5] = 'x';
	tftpTxFileName[6] = 't';
	parserStatus=PARSER_NOT_STARTED;

	//We allocate memory for the TFTP data buffer
	tftpBuffer = (BYTE *) malloc(TFTP_BUFFER_SIZE);
}

// This dummy TFTP program requests a file and then echoes it back
// If the file is received, isFileRx is set to TRUE
// After the echo back, isTFTPActivated is set to FALSE again
//and doesn't begin again until another start button push
void TFTPTask() {
	static enum {
		TFTP_CLIENT_IDLE,
		TFTP_CLIENT_WAIT,
		TFTP_CLIENT_READY,
		TFTP_CLIENT_READ_BOOTFILE,
		TFTP_CLIENT_DONE
	} clientTFTPState = TFTP_CLIENT_IDLE;
	static TFTP_RESULT result;

	switch (clientTFTPState) {

	case TFTP_CLIENT_IDLE:
		// On startup, we try to resolve TFTP server IP address.
		// Application may decide to do this when actual TFTP operation is required
		// This is completely application dependent.
		// By doing it here, we can reuse ARP on subsequent TFTP operation
		TFTPOpen(&AppConfig.PrimaryTFTPServer);

		//We initialize the pointer to the buffer array
		ParserInit(tftpBuffer);

		// Now we must wait for ARP to get resolved.
		clientTFTPState = TFTP_CLIENT_WAIT;
		break;

	case TFTP_CLIENT_WAIT:
		// Check to see if connection was established.
		result = TFTPIsOpened();
		if (result == TFTP_OK)
			// ARP is resolved and UDP port is opened.
			// Now we are ready for future TFTP commands.
			clientTFTPState = TFTP_CLIENT_READY;

		else if (result == TFTP_TIMEOUT)
			// Timeout has occurred.
			// Application may decide to count attempts and give-up after
			// so many retries or as in this case, continue to try forever.
			// We must have resolved server IP address or else nothing
			// can be done.
			clientTFTPState = TFTP_CLIENT_IDLE;
		break;

	case TFTP_CLIENT_READY:
		//Request the bootfile from the TFTP server
		if (isTFTPActivated) {
				clientTFTPState = TFTP_CLIENT_READ_BOOTFILE;
		}
		break;

	case TFTP_CLIENT_READ_BOOTFILE:
		// This function too may take time to finish.
		//It can return TRUE in case of error or timeout
		//so we have to check if the files has successfully been parsed
		if (TFTPRead()) {
			if(parserStatus==PARSER_END_OF_CODE){
				// Read has finished, go back to awaiting a button push
				clientTFTPState = TFTP_CLIENT_DONE;
				isFileRx = TRUE;
				isTFTPActivated = FALSE;
			}
			//There has been an error, we will restart the transfer
			else{
				clientTFTPState=TFTP_CLIENT_IDLE;
			}
		}
		break;

	case TFTP_CLIENT_DONE:
		//Do nothing, TFTP has fulfilled its task
		break;

	default:
		clientTFTPState=TFTP_CLIENT_IDLE;
		break;
	}

}

//This function attempts to read the file whose filename
//is stored in AppConfig.BootFileName and saves the data in tftpBuffer
static BOOL TFTPRead() {
	BOOL lbReturn;
	TFTP_RESULT result;
	BYTE v;
	TFTP_STATE tftpState;
	static BYTE * currentPtr;

	// State machine states.
	static enum {
		SM_TFTP_GET_IDLE,
		SM_TFTP_GET_WAIT,
		SM_TFTP_GET_OPEN_FILE,
		SM_TFTP_GET_OPEN_FILE_WAIT,
		SM_TFTP_GET_DATA
	} smTFTPGet = SM_TFTP_GET_OPEN_FILE;

	lbReturn = FALSE;

	switch (smTFTPGet) {

	case SM_TFTP_GET_OPEN_FILE:
		// Is is okay to issue file open?
		if (TFTPIsFileOpenReady()) {
			// Issue file open command.
			// For read operation, there is no need to data retry. All retries
			// are done automatically by module.
			TFTPOpenFile(AppConfig.BootFileName, TFTP_FILE_MODE_READ);

			currentPtr = tftpBuffer;

			// Wait for file get opened.
			smTFTPGet = SM_TFTP_GET_OPEN_FILE_WAIT;
		}
		break;

	case SM_TFTP_GET_OPEN_FILE_WAIT:
		// Is file opened?
		result = TFTPIsFileOpened();
		if (result == TFTP_OK)
			// Yes.  Go and read the first data block.
			smTFTPGet = SM_TFTP_GET_DATA;

		else if (result == TFTP_RETRY) {
			// Somehow fileopen was not acknowledged, try again.
			smTFTPGet = SM_TFTP_GET_OPEN_FILE;
			break;
		} else
			break;

	case SM_TFTP_GET_DATA:
		// Once a data block is available, application may read up to 512 bytes
		// of data.  Read up to 512 bytes in one shot.
		while (1) {
			//When the client must send an ACK, it means
			//that a data block was correctly received
			//so we can parse the code.
			tftpState = TFTPGetState();
			if (tftpState == SM_TFTP_SEND_ACK && !blockRead) {
				parserStatus=ParserTask(bytesReceived);
				blockRead = TRUE;
				bytesReceived = 0;
				//We already got a block, so we reset the buffer
				currentPtr = tftpBuffer;
			} else if (tftpState == SM_TFTP_WAIT_FOR_DATA) {
				blockRead = FALSE;
			}

			// Make sure that it is okay to call TFTPGet.  If all 512 bytes are
			// read or if there is no more data byte, this function will return
			// TFTP_NOT_READY.
			result = TFTPIsGetReady();

			if (result == TFTP_OK) {
				// Fetch data byte.
				*currentPtr++ =TFTPGet();
				bytesReceived++;
			} else {
				break;
			}
		}

		if (result == TFTP_END_OF_FILE && !blockRead) {
			parserStatus=ParserTask(bytesReceived);
			blockRead = TRUE;
		}

		break;
	}

	// Common logic for all states.
	if (result == TFTP_END_OF_FILE || result == TFTP_TIMEOUT
			|| result == TFTP_ERROR) {
		lbReturn = TRUE;
	}

	// On completion of command, reset the state machine.
	if (lbReturn)
		smTFTPGet = SM_TFTP_GET_OPEN_FILE;

	return lbReturn;
}

//// Port1 Interrupt for Button press, GDO2 from CC1101 and INT from enc28j60
//#pragma vector=PORT1_VECTOR
//__interrupt void PORT1_ISR(void) {
//// Button Interrupt
//	if (BUTTON_IFG & BUTTON) {
//		BUTTON_IFG &= ~BUTTON;
//		BUTTON_IE &= ~BUTTON; // Debounce
//		WDTCTL = WDT_ADLY_250; // 250ms assuming 32768Hz for ACLK
//		SFRIFG1 &= ~WDTIFG; // clear interrupt flag
//		SFRIE1 |= WDTIE;
//
//		if (applicationMode == APP_STANDBY_MODE) {
//			applicationMode = APP_NORMAL_MODE; // Switch from STANDBY to APPLICATION MODE
//			LED_OUT &= ~(LED1 + LED2);
//			__bic_SR_register_on_exit(LPM3_bits);
//		}
//
//		//A second pressing of the button enables the TFTP client
//		else if (applicationMode == APP_NORMAL_MODE && !StackIsInConfigMode()) {
//			isTFTPActivated = TRUE;
//		}
//	}
//}
//
//// WDT Interrupt Service Routine used to de-bounce button press
//#pragma vector=WDT_VECTOR
//__interrupt void WDT_ISR(void) {
//	SFRIE1 &= ~WDTIE; // disable interrupt
//	SFRIFG1 &= ~WDTIFG; // clear interrupt flag
//	WDTCTL = WDTPW + WDTHOLD; // put WDT back in hold state
//	BUTTON_IE |= BUTTON; // Debouncing complete
//}
