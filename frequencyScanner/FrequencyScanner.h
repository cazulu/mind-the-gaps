/*
 * FrequencyScanner.h
 *
 *  Created on: 31/10/2012
 *      Author: T
 */

#ifndef FREQUENCYSCANNER_H_
#define FREQUENCYSCANNER_H_

#include "GenericTypeDefs.h"
#include "cc1101/TI_CC_CC1100-CC2500.h"

//Definition of all the possible
//modulation formats for the scan
#define	SCAN_MOD_2FSK	0		//2-FSK
#define	SCAN_MOD_GFSK	1		//GFSK
#define	SCAN_MOD_ASK	2		//ASK
#define	SCAN_MOD_OOK	3 		//OOK
#define	SCAN_MOD_4FSK	4		//4FSK
#define	SCAN_MOD_MSK	5		//MSK

typedef struct {
	//Start of the frequency range to scan = startFreqMhz + startFreqKhz/1000 [MHz]
	UINT16 startFreqMhz;
	UINT16 startFreqKhz;
	//End of the frequency range to scan = stopFreqMhz + stopFreqKhz/1000 [MHz]
	UINT16 stopFreqMhz;
	UINT16 stopFreqKhz;
	UINT16 freqResolution; 	//Resolution of the scan(channel filter bandwidth) in kHz
	BYTE modFormat; 			//Modulation format assumed for the scan
	BYTE activateAGC;		//Enable/Disable Automatic Gain Control
	BYTE agcLnaGain;		//Gain of the Low Noise Amplifier 1 stage of the AGC, ignored if AGC on
	BYTE agcLna2Gain;		//Gain of the Low Noise Amplifier 2 stage of the AGC, ignored if AGC on
	BYTE agcDvgaGain;		//Gain of the Digital Variable Gain Amplifier stage of the AGC, ignored if AGC on
	BYTE padding;
	UINT16 rssiWait; 			//Time to wait before reading RSSI once setting a freq on the transceiver, in us
} SCAN_CONFIG;

//This function configures the SPI interface
//to the CC1101, resets the device and
//sets the scanning parameters
//GETS: Pointer to the scan parameters
//and the pointer to the structure
//where the Rssi values will be stored
//RETURNS: NULL if something went wrong
//or pointer to the SCAN_CONFIG structure
//with the parameters actually used by the function
SCAN_CONFIG * configScanner(SCAN_CONFIG * scanParameters, BYTE * rssiArray);

//Perform the scan, returns the amount of bytes stored in the rssiArray
unsigned int scanFreqBands(void);

#endif /* FREQUENCYSCANNER_H_ */
