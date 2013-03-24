/* --COPYRIGHT--,BSD
 * Copyright (c) 2011, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//----------------------------------------------------------------------------
//  Description:  This file contains definitions specific to the CC1100/2500.
//  The configuration registers, strobe commands, and status registers are 
//  defined, as well as some common masks for these registers.
//
//  MSP430/CC1100-2500 Interface Code Library v1.0
//
//  K. Quiring
//  Texas Instruments, Inc.
//  July 2006
//  IAR Embedded Workbench v3.41
//----------------------------------------------------------------------------

#ifndef _TI_CC_CC1100_2500
#define _TI_CC_CC1100_2500

#include "GenericTypeDefs.h"


// Configuration Registers
#define TI_CCxxx0_IOCFG2       0x00        // GDO2 output pin configuration
#define TI_CCxxx0_IOCFG1       0x01        // GDO1 output pin configuration
#define TI_CCxxx0_IOCFG0       0x02        // GDO0 output pin configuration
#define TI_CCxxx0_FIFOTHR      0x03        // RX FIFO and TX FIFO thresholds
#define TI_CCxxx0_SYNC1        0x04        // Sync word, high byte
#define TI_CCxxx0_SYNC0        0x05        // Sync word, low byte
#define TI_CCxxx0_PKTLEN       0x06        // Packet length
#define TI_CCxxx0_PKTCTRL1     0x07        // Packet automation control
#define TI_CCxxx0_PKTCTRL0     0x08        // Packet automation control
#define TI_CCxxx0_ADDR         0x09        // Device address
#define TI_CCxxx0_CHANNR       0x0A        // Channel number
#define TI_CCxxx0_FSCTRL1      0x0B        // Frequency synthesizer control
#define TI_CCxxx0_FSCTRL0      0x0C        // Frequency synthesizer control
#define TI_CCxxx0_FREQ2        0x0D        // Frequency control word, high byte
#define TI_CCxxx0_FREQ1        0x0E        // Frequency control word, middle byte
#define TI_CCxxx0_FREQ0        0x0F        // Frequency control word, low byte
#define TI_CCxxx0_MDMCFG4      0x10        // Modem configuration
#define TI_CCxxx0_MDMCFG3      0x11        // Modem configuration
#define TI_CCxxx0_MDMCFG2      0x12        // Modem configuration
#define TI_CCxxx0_MDMCFG1      0x13        // Modem configuration
#define TI_CCxxx0_MDMCFG0      0x14        // Modem configuration
#define TI_CCxxx0_DEVIATN      0x15        // Modem deviation setting
#define TI_CCxxx0_MCSM2        0x16        // Main Radio Cntrl State Machine config
#define TI_CCxxx0_MCSM1        0x17        // Main Radio Cntrl State Machine config
#define TI_CCxxx0_MCSM0        0x18        // Main Radio Cntrl State Machine config
#define TI_CCxxx0_FOCCFG       0x19        // Frequency Offset Compensation config
#define TI_CCxxx0_BSCFG        0x1A        // Bit Synchronization configuration
#define TI_CCxxx0_AGCCTRL2     0x1B        // AGC control
#define TI_CCxxx0_AGCCTRL1     0x1C        // AGC control
#define TI_CCxxx0_AGCCTRL0     0x1D        // AGC control
#define TI_CCxxx0_WOREVT1      0x1E        // High byte Event 0 timeout
#define TI_CCxxx0_WOREVT0      0x1F        // Low byte Event 0 timeout
#define TI_CCxxx0_WORCTRL      0x20        // Wake On Radio control
#define TI_CCxxx0_FREND1       0x21        // Front end RX configuration
#define TI_CCxxx0_FREND0       0x22        // Front end TX configuration
#define TI_CCxxx0_FSCAL3       0x23        // Frequency synthesizer calibration
#define TI_CCxxx0_FSCAL2       0x24        // Frequency synthesizer calibration
#define TI_CCxxx0_FSCAL1       0x25        // Frequency synthesizer calibration
#define TI_CCxxx0_FSCAL0       0x26        // Frequency synthesizer calibration
#define TI_CCxxx0_RCCTRL1      0x27        // RC oscillator configuration
#define TI_CCxxx0_RCCTRL0      0x28        // RC oscillator configuration
#define TI_CCxxx0_FSTEST       0x29        // Frequency synthesizer cal control
#define TI_CCxxx0_PTEST        0x2A        // Production test
#define TI_CCxxx0_AGCTEST      0x2B        // AGC test
#define TI_CCxxx0_TEST2        0x2C        // Various test settings
#define TI_CCxxx0_TEST1        0x2D        // Various test settings
#define TI_CCxxx0_TEST0        0x2E        // Various test settings

// Strobe commands
#define TI_CCxxx0_SRES         0x30        // Reset chip.
#define TI_CCxxx0_SFSTXON      0x31        // Enable/calibrate freq synthesizer
#define TI_CCxxx0_SXOFF        0x32        // Turn off crystal oscillator.
#define TI_CCxxx0_SCAL         0x33        // Calibrate freq synthesizer & disable
#define TI_CCxxx0_SRX          0x34        // Enable RX.
#define TI_CCxxx0_STX          0x35        // Enable TX.
#define TI_CCxxx0_SIDLE        0x36        // Exit RX / TX
#define TI_CCxxx0_SAFC         0x37        // AFC adjustment of freq synthesizer
#define TI_CCxxx0_SWOR         0x38        // Start automatic RX polling sequence
#define TI_CCxxx0_SPWD         0x39        // Enter pwr down mode when CSn goes hi
#define TI_CCxxx0_SFRX         0x3A        // Flush the RX FIFO buffer.
#define TI_CCxxx0_SFTX         0x3B        // Flush the TX FIFO buffer.
#define TI_CCxxx0_SWORRST      0x3C        // Reset real time clock.
#define TI_CCxxx0_SNOP         0x3D        // No operation.

// Status registers
#define TI_CCxxx0_PARTNUM      0x30        // Part number
#define TI_CCxxx0_VERSION      0x31        // Current version number
#define TI_CCxxx0_FREQEST      0x32        // Frequency offset estimate
#define TI_CCxxx0_LQI          0x33        // Demodulator estimate for link quality
#define TI_CCxxx0_RSSI         0x34        // Received signal strength indication
#define TI_CCxxx0_MARCSTATE    0x35        // Control state machine state
#define TI_CCxxx0_WORTIME1     0x36        // High byte of WOR timer
#define TI_CCxxx0_WORTIME0     0x37        // Low byte of WOR timer
#define TI_CCxxx0_PKTSTATUS    0x38        // Current GDOx status and packet status
#define TI_CCxxx0_VCO_VC_DAC   0x39        // Current setting from PLL cal module
#define TI_CCxxx0_TXBYTES      0x3A        // Underflow and # of bytes in TXFIFO
#define TI_CCxxx0_RXBYTES      0x3B        // Overflow and # of bytes in RXFIFO
#define TI_CCxxx0_NUM_RXBYTES  0x7F        // Mask "# of bytes" field in _RXBYTES

// Other memory locations
#define TI_CCxxx0_PATABLE      0x3E
#define TI_CCxxx0_TXFIFO       0x3F
#define TI_CCxxx0_RXFIFO       0x3F

// Masks for appended status bytes
#define TI_CCxxx0_LQI_RX       0x01        // Position of LQI byte
#define TI_CCxxx0_CRC_OK       0x80        // Mask "CRC_OK" bit within LQI byte

// Definitions to support burst/single access:
#define TI_CCxxx0_WRITE_BURST  0x40
#define TI_CCxxx0_READ_SINGLE  0x80
#define TI_CCxxx0_READ_BURST   0xC0


//----------------------------------------------------------------------------------
// MDMCFG2 - Modem Configuration #2 Register Values
//----------------------------------------------------------------------------------

//Mask to get the modulation format bits
#define TI_CCxxx0_MDMCFG2_MODFORMAT_MASK	0x70
//Modulation format of the radio signal
#define TI_CCxxx0_MDMCFG2_MODFORMAT_2FSK	0x00
#define TI_CCxxx0_MDMCFG2_MODFORMAT_GFSK	0x10
#define TI_CCxxx0_MDMCFG2_MODFORMAT_ASK		0x30
#define TI_CCxxx0_MDMCFG2_MODFORMAT_OOK		0x30
#define TI_CCxxx0_MDMCFG2_MODFORMAT_4FSK	0x40
#define TI_CCxxx0_MDMCFG2_MODFORMAT_MSK		0x70

//----------------------------------------------------------------------------------
// MDMCFG1 - Modem Configuration #1 Register Values
//----------------------------------------------------------------------------------

//Mask to get the channel spacing exponent bits
#define TI_CCxxx0_MDMCFG1_CHANSPC_E_MASK	0x02

//----------------------------------------------------------------------------------
// MDMCFG4 - Modem Configuration #4 Register Values
//----------------------------------------------------------------------------------

//Mask to get the channel bandwidth exponent bits
#define TI_CCxxx0_MDMCFG4_CHANBW_E	0xC0
//Mask to get the channel bandwidth mantissa bits
#define TI_CCxxx0_MDMCFG4_CHANBW_M	0x30


//----------------------------------------------------------------------------------
// AGCCTRL0 - AGC Control #0 Register
//----------------------------------------------------------------------------------

//AGCCTRL0 fields
#define TI_CCxxx0_AGCCTRL0_AGCFREEZE_MASK	0x0C

//Possible AGC_FREEZE field values
#define TI_CCxxx0_AGCCTRL0_AGCFREEZE_NOP	0x00	//Normal operation -> Always adjust gain when required
#define TI_CCxxx0_AGCCTRL0_AGCFREEZE_SYNC	0x04	//The gain setting is frozen when a sync word has been found
#define TI_CCxxx0_AGCCTRL0_AGCFREEZE_ANALOG	0x08	//Manually freeze the analogue gain setting and continue to adjust the digital gain
#define TI_CCxxx0_AGCCTRL0_AGCFREEZE_ALL	0x0C	//Manually freeze both the analogue and the digital gain setting


//----------------------------------------------------------------------------------
// AGCTEST - AGC Test Register - Control the gain of the LNA, LNA2 and DVGA modules when AGC frozen
//	More info available on http://e2e.ti.com/support/low_power_rf/f/155/p/180664/730390.aspx#730390
//----------------------------------------------------------------------------------

//AGCTEST fields
#define TI_CCxxx0_AGCTEST_DVGA_GAIN_MASK	0x07	//Digital gain setting -> Reset value:111
#define TI_CCxxx0_AGCTEST_LNA2_CURRENT_MASK	0x38	//LNA2 gain setting (000:Minimum - 111:Maximum) -> Reset value:111
#define TI_CCxxx0_AGCTEST_LNA_PD_GAIN_MASK	0xC0	//LNA gain setting(00:Maximum - 10:Minimum) -> Reset value:00

//AGCTEST values
#define TI_CCxxx0_AGCTEST_MAX_GAIN	0x3F	//Register value for maximum gain settings
#define TI_CCxxx0_AGCTEST_MIN_GAIN  0xC0	//Register value for minimum gain settings

//----------------------------------------------------------------------------------
// MARCSTATE Status Register Values
//----------------------------------------------------------------------------------

//Mask to ignore the higher 3 unused bits, AND with MARCSTATE
#define TI_CCxxx0_MARCSTATE_MASK				0x1F

//MARCSTATE values
#define TI_CCxxx0_MARCSTATE_SM_SLEEP			0x00
#define TI_CCxxx0_MARCSTATE_SM_IDLE				0x01
#define TI_CCxxx0_MARCSTATE_SM_XOFF				0x02
#define TI_CCxxx0_MARCSTATE_SM_VCOON_MC			0x03
#define TI_CCxxx0_MARCSTATE_SM_REGON_MC			0x04
#define TI_CCxxx0_MARCSTATE_SM_MANCAL			0x05
#define TI_CCxxx0_MARCSTATE_SM_VCOON			0x06
#define TI_CCxxx0_MARCSTATE_SM_REGON			0x07
#define TI_CCxxx0_MARCSTATE_SM_STARTCAL			0x08
#define TI_CCxxx0_MARCSTATE_SM_BWBOOST			0x09
#define TI_CCxxx0_MARCSTATE_SM_FS_LOCK			0x0A
#define TI_CCxxx0_MARCSTATE_SM_IFADCON			0x0B
#define TI_CCxxx0_MARCSTATE_SM_ENDCAL			0x0C
#define TI_CCxxx0_MARCSTATE_SM_RX				0x0D
#define TI_CCxxx0_MARCSTATE_SM_RX_END			0x0E
#define TI_CCxxx0_MARCSTATE_SM_RX_RST			0x0F
#define TI_CCxxx0_MARCSTATE_SM_TXRX_SWITCH		0x10
#define TI_CCxxx0_MARCSTATE_SM_RXFIFO_OVERFLOW	0x11
#define TI_CCxxx0_MARCSTATE_SM_FSTXON			0x12
#define TI_CCxxx0_MARCSTATE_SM_TX				0x13
#define TI_CCxxx0_MARCSTATE_SM_TX_END			0x14
#define TI_CCxxx0_MARCSTATE_SM_RXTX_SWITCH		0x15
#define TI_CCxxx0_MARCSTATE_SM_TXFIFO_UNDERFLOW	0x16


//----------------------------------------------------------------------------------
// Chip Status Byte
//----------------------------------------------------------------------------------

//Define the chip status byte data type
typedef BYTE RF_STATUS;

// Bit fields in the chip status byte
#define TI_CCxxx0_STATUS_CHIP_RDYn_BM             0x80
#define TI_CCxxx0_STATUS_STATE_BM                 0x70
#define TI_CCxxx0_STATUS_FIFO_BYTES_AVAILABLE_BM  0x0F

// Chip states
#define TI_CCxxx0_STATE_IDLE                      0x00
#define TI_CCxxx0_STATE_RX                        0x10
#define TI_CCxxx0_STATE_TX                        0x20
#define TI_CCxxx0_STATE_FSTXON                    0x30
#define TI_CCxxx0_STATE_CALIBRATE                 0x40
#define TI_CCxxx0_STATE_SETTLING                  0x50
#define TI_CCxxx0_STATE_RX_OVERFLOW               0x60
#define TI_CCxxx0_STATE_TX_UNDERFLOW              0x70


//----------------------------------------------------------------------------------
// Other register bit fields
//----------------------------------------------------------------------------------
#define TI_CCxxx0_LQI_CRC_OK_BM                   0x80
#define TI_CCxxx0_LQI_EST_BM                      0x7F

#endif

