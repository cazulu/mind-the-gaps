/*
 * FrequencyScanner.c
 *
 *  Created on: 31/10/2012
 *      Author: T
 */

#include <stdlib.h>
#include "FrequencyScanner.h"
#include "cc1101/TI_CC_spi.h"
#include "sleep.h"

/*** Definition of device characteristics ***/
//Define device type here
#define CC1101 1
#define CC1120 2
#define CC2500 3
#define CC2520 4

//The actual device type that we're using
#define CC_TRANSCEIVER CC1101

#if CC_TRANSCEIVER==CC1101

//---RF limits---
//Lower limit of the device's frequency range(MHz)
#define MIN_FREQ 779
//Upper limit of the device's frequency range(MHz)
#define MAX_FREQ 928

//List of valid channel filter bandwidth values(kHz)
//(controls the frequency resolution of the scan)
UINT16 filterBandwidthList[][4] = 	{{58,  68,  81,  102 },
									{ 116, 135, 162, 203 },
									{ 232, 270, 325, 406 },
									{ 464, 541, 650, 812 } };

#define DEFAULT_RESOLUTION 203

//Mapping between filter bandwidth values and register values
BYTE chanBwE[][4] = {{3, 3, 3, 3},
				 	{2, 2, 2, 2},
				 	{1, 1, 1, 1},
				 	{0, 0, 0, 0}};
BYTE chanBwM[][4] = {{3, 2, 1, 0},
					{3, 2, 1, 0},
					{3, 2, 1, 0},
					{3, 2, 1, 0}};

//Mapping between filter bandwidth and channel spacing registers
//BYTE chanSpcE[][]= 	{{58,  68,  81,  102 },
//					{ 116, 135, 162, 203 },
//					{ 232, 270, 325, 406 },
//					{ 464, 541, 650, 812 } };
//BYTE chanSpcM[][]= 	{{58,  68,  81,  102 },
//					{ 116, 135, 162, 203 },
//					{ 232, 270, 325, 406 },
//					{ 464, 541, 650, 812 } };

//List of valid modulation formats for the device
//in the order of the SCAN_MOD_FORMAT enum
BOOL validModFormats[] = { TRUE, //2-FSK
							TRUE, //GFSK
							TRUE, //ASK
							TRUE, //OOK
							TRUE, //4FSK
							TRUE }; //MSK
//Mapping between modulation format and the MOD_FORMAT field of the MDMCFG2 register
BYTE modFormatMDMCFG2[] = { TI_CCxxx0_MDMCFG2_MODFORMAT_2FSK, //2-FSK
							TI_CCxxx0_MDMCFG2_MODFORMAT_GFSK, //GFSK
							TI_CCxxx0_MDMCFG2_MODFORMAT_ASK, //ASK
							TI_CCxxx0_MDMCFG2_MODFORMAT_OOK, //OOK
							TI_CCxxx0_MDMCFG2_MODFORMAT_4FSK, //4FSK
							TI_CCxxx0_MDMCFG2_MODFORMAT_MSK }; //MSK

//AGCTEST values for static gain
#define LNA_MIN_GAIN	2
#define LNA_MAX_GAIN	0
#define LNA2_MIN_GAIN	0
#define LNA2_MAX_GAIN	7
#define DVGA_MIN_GAIN	0
#define DVGA_MAX_GAIN	7


/*---RSSI-related parameters---*/
//Wait time for RSSI, remember DN505
#define MIN_WAIT_WITH_AGC	328
#define AVG_WAIT_WITH_AGC	1000
#define MAX_WAIT_WITH_AGC	5000

#define MIN_WAIT_WITHOUT_AGC	250
#define AVG_WAIT_WITHOUT_AGC	250
#define MAX_WAIT_WITHOUT_AGC	5000

//RSSI offset of device, in this case constant for all frequencies
UINT8 rssiOffset=74;

//TEST0 parameters
#define NUM_TEST0_RANGES 2
UINT16 test0RangeLimits[]={779,861,928};
BYTE test0Values[]={0x0B,0x09};

//FSCAL2 parameters
#define NUM_FSCAL2_RANGES 2
UINT16 fscal2RangeLimits[]={779,861,928};

//Value of the crystal oscillator(XOSC) of the RF daughterboard
UINT32 fXoscMhz=26;

#endif



//Array to store the rssi values of each channel in dBm
static BYTE * rssiTable = NULL;

//Struct with the parameters currently in use by the scanner
static SCAN_CONFIG scanParameters;

//Value of the FREQ2:FREQ1:FREQ0 registers for the startFrequency
static UINT32 freqRegsBase;

//Step value of the FREQ2:FREQ1:FREQ0 registers
static UINT32 freqRegsStep;

//This function configures the SPI interface
//to the CC1101, resets the device and
//sets the scanning parameters
//GETS: Pointer to the scan parameters
//and the pointer to the structure
//where the Rssi values will be stored
//RETURNS: NULL if something went wrong
//or pointer to the SCAN_CONFIG structure
//with the parameters actually used by the function
SCAN_CONFIG * configScanner(SCAN_CONFIG * pconfigParameters, BYTE * rssiArray) {
	BYTE reg;

	//Store the array pointer, if it's NULL report failure
	if(rssiArray==NULL)
		return NULL;
	rssiTable = rssiArray;
	//Configure the SPI interface
	TI_CC_SPISetup();
	//Reset the CC1101
	TI_CC_PowerupResetCCxxxx();

	//If pconfigParameters is NULL, use defaults
	if(pconfigParameters==NULL){
		pconfigParameters=&scanParameters;
		scanParameters.startFreqMhz=MIN_FREQ;
		scanParameters.startFreqKhz=0;
		scanParameters.stopFreqMhz=MAX_FREQ;
		//scanParameters.stopFreqMhz=820;
		scanParameters.stopFreqKhz=0;
		scanParameters.freqResolution=DEFAULT_RESOLUTION;
		scanParameters.modFormat=TI_CCxxx0_MDMCFG2_MODFORMAT_ASK;
		scanParameters.rssiWait=AVG_WAIT_WITH_AGC;
		scanParameters.activateAGC=TRUE;
	}

	/**Start parsing parameters**/

	//Parsing startFreq
	if (pconfigParameters->startFreqMhz < MIN_FREQ
			|| pconfigParameters->startFreqMhz > MAX_FREQ) {
		//Use default value: MIN_FREQ
		scanParameters.startFreqMhz = MIN_FREQ;
		scanParameters.startFreqKhz = 0;
	} else {
		scanParameters.startFreqMhz = pconfigParameters->startFreqMhz;
		if (pconfigParameters->startFreqKhz>= 1000
				|| pconfigParameters->startFreqMhz==MAX_FREQ) {
			//Use default value: 0
			scanParameters.startFreqKhz = 0;
		} else{
			scanParameters.startFreqKhz = pconfigParameters->startFreqKhz;
		}
	}

		//Parsing stopFreq
	if (pconfigParameters->stopFreqMhz < MIN_FREQ
			|| pconfigParameters->stopFreqMhz > MAX_FREQ) {
		//Use default value: MAX_FREQ
		scanParameters.stopFreqMhz = MAX_FREQ;
		scanParameters.stopFreqKhz = 0;
	} else {
		scanParameters.stopFreqMhz = pconfigParameters->stopFreqMhz;
		if (pconfigParameters->stopFreqKhz>= 1000
				|| pconfigParameters->stopFreqMhz==MAX_FREQ) {
			//Use default value: 0
			scanParameters.stopFreqKhz = 0;
		} else{
			scanParameters.stopFreqKhz = pconfigParameters->stopFreqKhz;
		}
	}
		//Check if the range limits make sense
	if (scanParameters.startFreqMhz > scanParameters.stopFreqMhz)
		return NULL;
	if (scanParameters.startFreqMhz == scanParameters.stopFreqMhz
			&& scanParameters.startFreqKhz > scanParameters.stopFreqKhz)
		return NULL;
	if (scanParameters.startFreqMhz == MAX_FREQ
			&& scanParameters.startFreqKhz > 0)
		return NULL;

	//Parsing filterBandwidth
	if(pconfigParameters->freqResolution<filterBandwidthList[0][0] ||
			pconfigParameters->freqResolution>filterBandwidthList[3][3])
		scanParameters.freqResolution=DEFAULT_RESOLUTION;

	//Find the valid filter value closest to the one requested
	UINT16 diff, tempDiff;
	BYTE i, j, row, column;
	for (i = 0; i < sizeof(filterBandwidthList); i++) {
		for (j = 0; j < sizeof(*filterBandwidthList); j++) {
			if(filterBandwidthList[i][j] > pconfigParameters->freqResolution)
				tempDiff=filterBandwidthList[i][j] - pconfigParameters->freqResolution;
			else
				tempDiff=pconfigParameters->freqResolution - filterBandwidthList[i][j];
			if (i == 0 && j==0)
				diff = tempDiff;
			if (tempDiff <= diff) {
				diff = tempDiff;
				row = i;
				column = j;
			}
			if (diff == 0)
				break;
		}
		if (diff == 0)
			break;
	}
	scanParameters.freqResolution = filterBandwidthList[row][column];
	//Set the channel filter bandwidth
	reg = TI_CC_SPIReadReg(TI_CCxxx0_MDMCFG4);
	reg &= ~(TI_CCxxx0_MDMCFG4_CHANBW_E + TI_CCxxx0_MDMCFG4_CHANBW_M);
	reg |= (chanBwE[row][column] << 4) + (chanBwM[row][column] << 6);
	TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG4, reg);
	//Initialize the variables related to the frequency registers REG2:REG1:REG0
	//Formula: f_carrier=(f_xosc/2^16)*FREQ[23:0]
	//TODO: To optimize, consider using Horner division code from SLAA329
	//Link: http://www.ti.com/mcu/docs/litabsmultiplefilelist.tsp?sectionId=96&tabId=1502&literatureNumber=slaa329&docCategoryId=1&familyId=4
	freqRegsBase=(((UINT32)scanParameters.startFreqMhz)<<16)/fXoscMhz;
	freqRegsBase+=(((UINT32)scanParameters.startFreqKhz)<<16)/(fXoscMhz*1000);
	freqRegsStep=(((UINT32)scanParameters.freqResolution)<<16)/(fXoscMhz*1000);


	//Set the channel spacing
//	reg = TI_CC_SPIReadReg(TI_CCxxx0_MDMCFG1);
//	reg &= ~TI_CCxxx0_MDMCFG1_CHANSPC_E_MASK;
//	reg |= chanSpcE[row][column];
//	TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG1, reg);
//	TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG0, chanSpcM[row][column]);
	//Set the number of channels per Mhz
	//Parsing the modulation format
	//If it's a wrong or invalid value, use the first valid modulation format
	if (pconfigParameters->modFormat < sizeof(validModFormats)
			&& validModFormats[pconfigParameters->modFormat])
		scanParameters.modFormat = pconfigParameters->modFormat;
	else {
		for (i = 0; i < sizeof(validModFormats) && !validModFormats[i]; i++)
			;
		scanParameters.modFormat = i;
	}
	//Set the modulation for the current scan
	reg = TI_CC_SPIReadReg(TI_CCxxx0_MDMCFG2);
	reg &= ~TI_CCxxx0_MDMCFG2_MODFORMAT_MASK;
	reg |= modFormatMDMCFG2[scanParameters.modFormat];
	TI_CC_SPIWriteReg(TI_CCxxx0_MDMCFG2, reg);

	//Parse AGC options
	//If the AGC is disabled, we manually set
	//all the gain stages, otherwise we ignore
	//those fields
	if(pconfigParameters->activateAGC){
		scanParameters.activateAGC = TRUE;
		//Parse rssiWait
		if(pconfigParameters->rssiWait<MIN_WAIT_WITH_AGC || pconfigParameters->rssiWait>MAX_WAIT_WITH_AGC){
			//Use default: AVG_WAIT_WITHOUT_AGC
			scanParameters.rssiWait=AVG_WAIT_WITH_AGC;
		}
		else{
			scanParameters.rssiWait=pconfigParameters->rssiWait;
		}
	}
	else{
		scanParameters.activateAGC = FALSE;
		//Freeze the AGC
		reg = TI_CC_SPIReadReg(TI_CCxxx0_AGCCTRL0);
		reg &= ~TI_CCxxx0_AGCCTRL0_AGCFREEZE_MASK;
		reg |= TI_CCxxx0_AGCCTRL0_AGCFREEZE_ALL;
		TI_CC_SPIWriteReg(TI_CCxxx0_AGCCTRL0, reg);
		//Parse static gain options
		if (pconfigParameters->agcLnaGain < LNA_MIN_GAIN
				&& pconfigParameters->agcLnaGain > LNA_MAX_GAIN)
			scanParameters.agcLnaGain = pconfigParameters->agcLnaGain;
		else
			scanParameters.agcLnaGain = LNA_MAX_GAIN;
		if (pconfigParameters->agcLna2Gain > LNA2_MIN_GAIN
				&& pconfigParameters->agcLna2Gain < LNA2_MAX_GAIN)
			scanParameters.agcLna2Gain = pconfigParameters->agcLna2Gain;
		else
			scanParameters.agcLna2Gain = LNA2_MAX_GAIN;
		if (pconfigParameters->agcDvgaGain > DVGA_MIN_GAIN
				&& pconfigParameters->agcDvgaGain < DVGA_MAX_GAIN)
			scanParameters.agcDvgaGain = pconfigParameters->agcDvgaGain;
		else
			scanParameters.agcDvgaGain = DVGA_MAX_GAIN;
		//Set static gain value
		reg = scanParameters.agcDvgaGain | (scanParameters.agcLna2Gain << 3)
				| (scanParameters.agcLna2Gain << 6);
		TI_CC_SPIWriteReg(TI_CCxxx0_AGCTEST, reg);

		//Parse rssiWait
		if(pconfigParameters->rssiWait<MIN_WAIT_WITHOUT_AGC || pconfigParameters->rssiWait>MAX_WAIT_WITHOUT_AGC){
			//Use default: AVG_WAIT_WITH_AGC
			scanParameters.rssiWait=AVG_WAIT_WITH_AGC;
		}
		else{
			scanParameters.rssiWait=pconfigParameters->rssiWait;
		}
	}
	/**Finished parsing parameters**/

	//Everything OK, return pointer to the parameter structure
	return &scanParameters;
}

//Perform the scan, returns the amount of bytes stored in the rssiArray
unsigned int scanFreqBands(void) {
	//Amount of bytes stored in the rssiArray
	unsigned int rssiLen=0;

	BYTE i;
	//RSSI value as reported by the CC device
	UINT8 rssi_dec;
	//RSSI value translated to dBm
	INT16 rssi_dBm;
	//Variable to store the current value of the TEST0 register
	BYTE currentTest0;
	//Variable to store the FREQ register value for the current frequency
	UINT32 freqRegs;
	//Temp variables for the FREQ register calculation
	BYTE tempFreq2,tempFreq1,tempFreq0;
	//Variables to store the current frequency to be scanned
	UINT16 currentFreqMhz, currentFreqKhz;
	//Controls if we should perform a calibration before starting RX
	//in the current frequency
	BOOL calNeeded;
	//Copy of the pointer to the RSSI array
	BYTE * rssiPtr=rssiTable;

	//Initialize the value of the frequency counters
	currentFreqMhz = scanParameters.startFreqMhz;
	currentFreqKhz = scanParameters.startFreqKhz;
	//Initialize freqRegs to the value for startFreq
	freqRegs=freqRegsBase;
	//Get reset value of TEST0
	currentTest0=TI_CC_SPIReadReg(TI_CCxxx0_TEST0);
	//For the start frequency a calibration is always needed
	calNeeded=TRUE;

	while (currentFreqMhz<scanParameters.stopFreqMhz || currentFreqKhz<scanParameters.stopFreqKhz) {
		//Find out if we need to change the TEST0 register
		for(i=0; i<NUM_TEST0_RANGES; i++){
			//Find the TEST0 range the current frequency belongs to
			if(currentFreqMhz>=test0RangeLimits[i] && currentFreqMhz<=test0RangeLimits[i+1]){
				if(currentTest0!=test0Values[i]){
					TI_CC_SPIWriteReg(TI_CCxxx0_TEST0, test0Values[i]);
					currentTest0=TI_CC_SPIReadReg(TI_CCxxx0_TEST0);
					//Also change the value of the FSCAL2 register to enable the VCO calibration stage
					TI_CC_SPIWriteReg(TI_CCxxx0_FSCAL2, 0x2A);
					calNeeded=TRUE;
				}
				break;
			}
		}

		//Write the frequency registers FREQ2, FREQ1 and FREQ0
		tempFreq0=(BYTE)freqRegs;
		tempFreq1=(BYTE)(freqRegs>>8);
		tempFreq2=(BYTE)(freqRegs>>16);
		TI_CC_SPIWriteReg(TI_CCxxx0_FREQ2, tempFreq2);
		TI_CC_SPIWriteReg(TI_CCxxx0_FREQ1, tempFreq1);
		TI_CC_SPIWriteReg(TI_CCxxx0_FREQ0, tempFreq0);

		//Calibrate if needed
		if(calNeeded){
			TI_CC_SPIStrobe(TI_CCxxx0_SCAL);
			calNeeded=FALSE;
		}

		//Enter RX mode by issuing an SRX strobe command
		TI_CC_SPIStrobe(TI_CCxxx0_SRX);

		static BYTE state;
		//Wait for radio to enter RX state by checking the status byte
		do {
			//state = TI_CC_GetTxStatus() & TI_CCxxx0_STATUS_STATE_BM;
			state=TI_CC_SPIReadStatus(TI_CCxxx0_MARCSTATE);
			state&=TI_CCxxx0_MARCSTATE_MASK;
			//Flush the FIFO RX buffer in case of overflow
			if(state==TI_CCxxx0_MARCSTATE_SM_RXFIFO_OVERFLOW)
				TI_CC_SPIStrobe(TI_CCxxx0_SFRX);
		} while (state != TI_CCxxx0_MARCSTATE_SM_RX);

		//Wait for RSSI to be valid
		usleep(scanParameters.rssiWait);

		//Enter IDLE state by issuing an SIDLE strobe command
		TI_CC_SPIStrobe(TI_CCxxx0_SIDLE);

		//Read RSSI value and store it in rssi_dec
		rssi_dec = TI_CC_SPIReadStatus(TI_CCxxx0_RSSI);

		//Store the value in the rssi array
		*rssiPtr++=rssi_dec;
		//Update rssiLen
		rssiLen+=sizeof(BYTE);

		//Flush the FIFO buffer, just in case
		TI_CC_SPIStrobe(TI_CCxxx0_SFRX);

		//At the end of the loop, update the frequency counters
		//TODO: Consider using Horner division code
		currentFreqKhz += scanParameters.freqResolution;
		if (currentFreqKhz >= 1000) {
			currentFreqMhz += currentFreqKhz/1000;
			currentFreqKhz %= 1000;
			//According to DN508, a frequency calibration
			//covers all frequencies less than 1Mhz apart
			//from the one we calibrated for
			calNeeded=TRUE;
		}
		//Update the value of the FREQ2:FREQ1:FREQ0 register value
		freqRegs+=freqRegsStep;
	}

	return rssiLen;
}

