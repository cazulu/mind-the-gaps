/*********************************************************************
 *
 *  Medium Access Control (MAC) Layer for Microchip ENC28J60
 *  Module for Microchip TCP/IP Stack
 *   -Provides access to ENC28J60 Ethernet controller
 *   -Reference: ENC28J60 Data sheet, IEEE 802.3 Standard
 *
 *********************************************************************
 * FileName:        ENC28J60.c
 * Dependencies:    ENC28J60.h
 *                  MAC.h
 *                  string.h
 *                  StackTsk.h
 *                  Helpers.h
 *                  Delay.h
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32
 * Compiler:        Microchip C32 v1.05 or higher
 *					Microchip C30 v3.12 or higher
 *					Microchip C18 v3.30 or higher
 *					HI-TECH PICC-18 PRO 9.63PL2 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * Copyright (C) 2002-2009 Microchip Technology Inc.  All rights
 * reserved.
 *
 * Microchip licenses to you the right to use, modify, copy, and
 * distribute:
 * (i)  the Software when embedded on a Microchip microcontroller or
 *      digital signal controller product ("Device") which is
 *      integrated into Licensee's product; or
 * (ii) ONLY the Software driver source files ENC28J60.c, ENC28J60.h,
 *		ENCX24J600.c and ENCX24J600.h ported to a non-Microchip device
 *		used in conjunction with a Microchip ethernet controller for
 *		the sole purpose of interfacing with the ethernet controller.
 *
 * You should refer to the license agreement accompanying this
 * Software for additional information regarding your rights and
 * obligations.
 *
 * THE SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT
 * WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * MICROCHIP BE LIABLE FOR ANY INCIDENTAL, SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF
 * PROCUREMENT OF SUBSTITUTE GOODS, TECHNOLOGY OR SERVICES, ANY CLAIMS
 * BY THIRD PARTIES (INCLUDING BUT NOT LIMITED TO ANY DEFENSE
 * THEREOF), ANY CLAIMS FOR INDEMNITY OR CONTRIBUTION, OR OTHER
 * SIMILAR COSTS, WHETHER ASSERTED ON THE BASIS OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE), BREACH OF WARRANTY, OR OTHERWISE.
 *
 *
 * Author               Date        Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Howard Schlunder     6/28/04 Original
 * Howard Schlunder     10/8/04 Cleanup
 * Howard Schlunder     10/19/04 Small optimizations and more cleanup
 * Howard Schlunder     11/29/04 Added Set/GetCLKOUT
 * Howard Schlunder     12/23/05 Added B1 silicon errata workarounds
 * Howard Schlunder     1/09/06 Added comments and minor mods
 * Howard Schlunder     1/18/06 Added more silicon errata workarounds
 * Howard Schlunder     6/16/06 Synchronized with PIC18F97J60 code
 * Howard Schlunder     7/17/06 Updated TestMemory() for C30
 * Howard Schlunder     8/07/06 Added SetRXHashTableEntry() function
 * Javier Lara			29/09/12 Included MSP430 stuff
 ********************************************************************/
#ifndef __ENC28J60_C
#define __ENC28J60_C

#include "HardwareProfile.h"
#include "ENC28J60.h"

#include "TCPIP.h"

/** D E F I N I T I O N S ****************************************************/
// IMPORTANT SPI NOTE: The code in this file expects that the SPI interrupt
//      flag (ENC_SPI_IF) be clear at all times.  If the SPI is shared with
//      other hardware, the other code should clear the ENC_SPI_IF when it is
//      done using the SPI.
// Since the ENC28J60 doesn't support auto-negotiation, full-duplex mode is
// not compatible with most switches/routers.  If a dedicated network is used
// where the duplex of the remote node can be manually configured, you may
// change this configuration.  Otherwise, half duplex should always be used.
//#define HALF_DUPLEX
#define FULL_DUPLEX
//#define LEDB_DUPLEX

// Pseudo Functions
#define LOW(a)                  ((a) & 0xFF)
#define HIGH(a)                 (((a)>>8) & 0xFF)

static BYTE debug = 0xFF;

// ENC28J60 Opcodes (to be ORed with a 5 bit address)
#define WCR (0x2<<5)            // Write Control Register command
#define BFS (0x4<<5)            // Bit Field Set command
#define BFC (0x5<<5)            // Bit Field Clear command
#define RCR (0x0<<5)            // Read Control Register command
#define RBM ((0x1<<5) | 0x1A)   // Read Buffer Memory command
#define WBM ((0x3<<5) | 0x1A)   // Write Buffer Memory command
#define SR  ((0x7<<5) | 0x1F)   // System Reset command does not use an address.
                                //   It requires 0x1F, however.

// Maximum SPI frequency specified in data sheet
#define ENC_MAX_SPI_FREQ    (20000000ul)    // Hz
#define ETHER_IP    (0x00u)
#define ETHER_ARP   (0x06u)

// A header appended at the start of all RX frames by the hardware
typedef struct {
	WORD NextPacketPointer;
	RXSTATUS StatusVector;

	MAC_ADDR DestMACAddr;
	MAC_ADDR SourceMACAddr;
	WORD_VAL Type;
} ENC_PREAMBLE;

#define spiWrite			spi_rw
#define spiRead()			spi_rw(0)

// Prototypes of functions intended for MAC layer use only.
void BankSel(WORD Register);
REG ReadETHReg(BYTE Address);
static REG ReadMACReg(BYTE Address);
static void WriteReg(BYTE Address, BYTE Data);
static void BFCReg(BYTE Address, BYTE Data);
static void BFSReg(BYTE Address, BYTE Data);
static void SendSystemReset(void);

// Internal MAC level variables and flags.
static WORD_VAL NextPacketLocation;
static WORD_VAL CurrentPacketLocation;
static BOOL WasDiscarded;
static BYTE ENCRevID;

//NOTE: All code in this module expects Bank 0 to be currently selected.  If code ever changes the bank, it must restore it to Bank 0 before returning.

/******************************************************************************
 * Function:        void MACInit(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACInit sets up the PIC's SPI module and all the
 *                  registers in the ENC28J60 so that normal operation can
 *                  begin.
 *
 * Note:            None
 *****************************************************************************/
void MACInit() {

	BYTE i;

	// RESET the entire ENC28J60, clearing all registers
	// Also wait for CLKRDY to become set.
	// Bit 3 in ESTAT is an unimplemented bit.  If it reads out as '1' that
	// means the part is in RESET or there is something wrong with the SPI
	// connection.  This loop makes sure that we can communicate with the
	// ENC28J60 before proceeding.
	SendSystemReset();
	do {
		i = ReadETHReg(ESTAT).Val;
	} while ((i & 0x08) || (~i & ESTAT_CLKRDY));

	// Start up in Bank 0 and configure the receive buffer boundary pointers
	// and the buffer write protect pointer (receive buffer read pointer)
	WasDiscarded = TRUE;
	NextPacketLocation.Val = RXSTART;

	WriteReg(ERXSTL, LOW(RXSTART));
	WriteReg(ERXSTH, HIGH(RXSTART));
	WriteReg(ERXRDPTL, LOW(RXSTOP)); // Write low byte first
	WriteReg(ERXRDPTH, HIGH(RXSTOP)); // Write high byte last
	WriteReg(ERXNDL, LOW(RXSTOP));
	WriteReg(ERXNDH, HIGH(RXSTOP));
	WriteReg(ETXSTL, LOW(TXSTART));
	WriteReg(ETXSTH, HIGH(TXSTART));

	// Write a permanent per packet control byte of 0x00
	WriteReg(EWRPTL, LOW(TXSTART));
	WriteReg(EWRPTH, HIGH(TXSTART));
	MACPut(0x00);

	// Enter Bank 1 and configure Receive Filters
	// (No need to reconfigure - Unicast OR Broadcast with CRC checking is
	// acceptable)
	// Write ERXFCON_CRCEN only to ERXFCON to enter promiscuous mode

	//TODO: Originally the following two lines were commented, this is an attempt not to discard broadcast packets
	// Promiscious mode example:
	BankSel(ERXFCON);
	//WriteReg((BYTE)ERXFCON, ERXFCON_CRCEN);
	WriteReg((BYTE)ERXFCON, 0x0000);

	// Enter Bank 2 and configure the MAC
	BankSel(MACON1);

	// Enable the receive portion of the MAC
	WriteReg((BYTE) MACON1, MACON1_TXPAUS | MACON1_RXPAUS | MACON1_MARXEN);

	// Pad packets to 60 bytes, add CRC, and check Type/Length field.
#if defined(FULL_DUPLEX)
	WriteReg((BYTE) MACON3,
			MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);
	WriteReg((BYTE) MABBIPG, 0x15);
#else
	WriteReg((BYTE)MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN);
	WriteReg((BYTE)MABBIPG, 0x12);
#endif

	// Allow infinite deferals if the medium is continuously busy
	// (do not time out a transmission if the half duplex medium is
	// completely saturated with other people's data)
	WriteReg((BYTE) MACON4, MACON4_DEFER);

	// Late collisions occur beyond 63+8 bytes (8 bytes for preamble/start of frame delimiter)
	// 55 is all that is needed for IEEE 802.3, but ENC28J60 B5 errata for improper link pulse
	// collisions will occur less often with a larger number.
	WriteReg((BYTE) MACLCON2, 63);

	// Set non-back-to-back inter-packet gap to 9.6us.  The back-to-back
	// inter-packet gap (MABBIPG) is set by MACSetDuplex() which is called
	// later.
	WriteReg((BYTE) MAIPGL, 0x12);
	WriteReg((BYTE) MAIPGH, 0x0C);

	// Set the maximum packet size which the controller will accept
	WriteReg((BYTE) MAMXFLL, LOW(6+6+2+1500+4)); // 1518 is the IEEE 802.3 specified limit
	WriteReg((BYTE) MAMXFLH, HIGH(6+6+2+1500+4)); // 1518 is the IEEE 802.3 specified limit

	// Enter Bank 3 and initialize physical MAC address registers
	BankSel(MAADR1);
	WriteReg((BYTE) MAADR1, AppConfig.MyMACAddr.v[0]);
	WriteReg((BYTE) MAADR2, AppConfig.MyMACAddr.v[1]);
	WriteReg((BYTE) MAADR3, AppConfig.MyMACAddr.v[2]);
	WriteReg((BYTE) MAADR4, AppConfig.MyMACAddr.v[3]);
	WriteReg((BYTE) MAADR5, AppConfig.MyMACAddr.v[4]);
	WriteReg((BYTE) MAADR6, AppConfig.MyMACAddr.v[5]);

	// Disable the CLKOUT output to reduce EMI generation
	WriteReg((BYTE) ECOCON, 0x00); // Output off (0V)
	//WriteReg((BYTE)ECOCON, 0x01); // 25.000MHz
	//WriteReg((BYTE)ECOCON, 0x03); // 8.3333MHz (*4 with PLL is 33.3333MHz)

	// Get the Rev ID so that we can implement the correct errata workarounds
	ENCRevID = ReadETHReg((BYTE) EREVID).Val;

	// Disable half duplex loopback in PHY.  Bank bits changed to Bank 2 as a
	// side effect.
	WritePHYReg(PHCON2, PHCON2_HDLDIS);

	// Configure LEDA to display LINK status, LEDB to display TX/RX activity
	SetLEDConfig(0x3472);

	// Set the MAC and PHY into the proper duplex state
#if defined(FULL_DUPLEX)
	WritePHYReg(PHCON1, PHCON1_PDPXMD);
#elif defined(HALF_DUPLEX)
	WritePHYReg(PHCON1, 0x0000);
#endif

	BankSel(ERDPTL); // Return to default Bank 0

	//TODO: Maybe re-enable Packet Pending Interrupt
//	//Clear the flag and enable the Receive Packet Pending interrupt
//	BFCReg(EIR, EIR_PKTIF);
//	BFSReg(EIE, EIE_INTIE|EIE_PKTIE);

	// Enable packet reception
	BFSReg(ECON1, ECON1_RXEN);
}

/******************************************************************************
 * Function:        BOOL MACIsLinked(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TRUE: If the PHY reports that a link partner is present
 *                        and the link has been up continuously since the last
 *                        call to MACIsLinked()
 *                  FALSE: If the PHY reports no link partner, or the link went
 *                         down momentarily since the last call to MACIsLinked()
 *
 * Side Effects:    None
 *
 * Overview:        Returns the PHSTAT1.LLSTAT bit.
 *
 * Note:            None
 *****************************************************************************/
BOOL MACIsLinked(void) {
	// LLSTAT is a latching low link status bit.  Therefore, if the link
	// goes down and comes back up before a higher level stack program calls
	// MACIsLinked(), MACIsLinked() will still return FALSE.  The next
	// call to MACIsLinked() will return TRUE (unless the link goes down
	// again).

	return ReadPHYReg((BYTE)PHSTAT1).PHSTAT1bits.LLSTAT;
}

/******************************************************************************
 * Function:        BOOL MACIsTxReady(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TRUE: If no Ethernet transmission is in progress
 *                  FALSE: If a previous transmission was started, and it has
 *                         not completed yet.  While FALSE, the data in the
 *                         transmit buffer and the TXST/TXND pointers must not
 *                         be changed.
 *
 * Side Effects:    None
 *
 * Overview:        Returns the ECON1.TXRTS bit
 *
 * Note:            None
 *****************************************************************************/
BOOL MACIsTxReady(void) {
	return !ReadETHReg(ECON1).ECON1bits.TXRTS;
}

/******************************************************************************
 * Function:        void MACDiscardRx(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Marks the last received packet (obtained using
 *                  MACGetHeader())as being processed and frees the buffer
 *                  memory associated with it
 *
 * Note:            Is is safe to call this function multiple times between
 *                  MACGetHeader() calls.  Extra packets won't be thrown away
 *                  until MACGetHeader() makes it available.
 *****************************************************************************/
void MACDiscardRx(void) {
	WORD_VAL NewRXRDLocation;

	// Make sure the current packet was not already discarded
	if (WasDiscarded)
		return;
	WasDiscarded = TRUE;

	// Decrement the next packet pointer before writing it into
	// the ERXRDPT registers.  This is a silicon errata workaround.
	// RX buffer wrapping must be taken into account if the
	// NextPacketLocation is precisely RXSTART.
	NewRXRDLocation.Val = NextPacketLocation.Val - 1;
	if (NewRXRDLocation.Val > RXSTOP) {
		NewRXRDLocation.Val = RXSTOP;
	}

	// Decrement the RX packet counter register, EPKTCNT
	BFSReg(ECON2, ECON2_PKTDEC);

	// Move the receive read pointer to unwrite-protect the memory used by the
	// last packet.  The writing order is important: set the low byte first,
	// high byte last.
	WriteReg(ERXRDPTL, NewRXRDLocation.v[0]);
	WriteReg(ERXRDPTH, NewRXRDLocation.v[1]);
}

/******************************************************************************
 * Function:        WORD MACGetFreeRxSize(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          A WORD estimate of how much RX buffer space is free at
 *                  the present time.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 *****************************************************************************/
WORD MACGetFreeRxSize(void) {
	WORD_VAL ReadPT, WritePT;

	// Read the Ethernet hardware buffer write pointer.  Because packets can be
	// received at any time, it can change between reading the low and high
	// bytes.  A loop is necessary to make certain a proper low/high byte pair
	// is read.
	BankSel(EPKTCNT);
	do {
		// Save EPKTCNT in a temporary location
		ReadPT.v[0] = ReadETHReg((BYTE) EPKTCNT).Val;

		BankSel(ERXWRPTL);
		WritePT.v[0] = ReadETHReg(ERXWRPTL).Val;
		WritePT.v[1] = ReadETHReg(ERXWRPTH).Val;

		BankSel(EPKTCNT);
	} while (ReadETHReg((BYTE) EPKTCNT).Val != ReadPT.v[0]);

	// Determine where the write protection pointer is
	BankSel(ERXRDPTL);
	ReadPT.v[0] = ReadETHReg(ERXRDPTL).Val;
	ReadPT.v[1] = ReadETHReg(ERXRDPTH).Val;

	// Calculate the difference between the pointers, taking care to account
	// for buffer wrapping conditions
	if (WritePT.Val > ReadPT.Val) {
		return (RXSTOP - RXSTART) - (WritePT.Val - ReadPT.Val);
	} else if (WritePT.Val == ReadPT.Val) {
		return RXSIZE - 1;
	} else {
		return ReadPT.Val - WritePT.Val - 1;
	}
}

/******************************************************************************
 * Function:        BOOL MACGetHeader(MAC_ADDR *remote, BYTE* type)
 *
 * PreCondition:    None
 *
 * Input:           *remote: Location to store the Source MAC address of the
 *                           received frame.
 *                  *type: Location of a BYTE to store the constant
 *                         MAC_UNKNOWN, ETHER_IP, or ETHER_ARP, representing
 *                         the contents of the Ethernet type field.
 *
 * Output:          TRUE: If a packet was waiting in the RX buffer.  The
 *                        remote, and type values are updated.
 *                  FALSE: If a packet was not pending.  remote and type are
 *                         not changed.
 *
 * Side Effects:    Last packet is discarded if MACDiscardRx() hasn't already
 *                  been called.
 *
 * Overview:        None
 *
 * Note:            None
 *****************************************************************************/
BOOL MACGetHeader(MAC_ADDR *remote, BYTE* type) {
	ENC_PREAMBLE header;
	BYTE PacketCount;

	// Test if at least one packet has been received and is waiting
	BankSel(EPKTCNT);
	PacketCount = ReadETHReg((BYTE) EPKTCNT).Val;
	BankSel(ERDPTL);
	if (PacketCount == 0u)
		return FALSE;

	// Make absolutely certain that any previous packet was discarded
	if (WasDiscarded == FALSE) {
		MACDiscardRx();
		return FALSE;
	}

	// Set the SPI read pointer to the beginning of the next unprocessed packet
	CurrentPacketLocation.Val = NextPacketLocation.Val;
	WriteReg(ERDPTL, CurrentPacketLocation.v[0]);
	WriteReg(ERDPTH, CurrentPacketLocation.v[1]);

	// Obtain the MAC header from the Ethernet buffer
	MACGetArray((BYTE*) &header, sizeof(header));

	// The EtherType field, like most items transmitted on the Ethernet medium
	// are in big endian.
	header.Type.Val = swaps(header.Type.Val);

//TODO: Maybe allow this dangerous reset
//	// Validate the data returned from the ENC28J60.  Random data corruption,
//	// such as if a single SPI bit error occurs while communicating or a
//	// momentary power glitch could cause this to occur in rare circumstances.
//	if (header.NextPacketPointer > RXSTOP
//			|| ((BYTE_VAL*) (&header.NextPacketPointer))->bits.b0
//			|| header.StatusVector.bits.Zero
//			|| header.StatusVector.bits.CRCError
//			|| header.StatusVector.bits.ByteCount > 1518u
//			|| !header.StatusVector.bits.ReceiveOk) {
//		Reset();
//	}

	// Save the location where the hardware will write the next packet to
	NextPacketLocation.Val = header.NextPacketPointer;

	// Return the Ethernet frame's Source MAC address field to the caller
	// This parameter is useful for replying to requests without requiring an
	// ARP cycle.
	memcpy((void*) remote->v, (void*) header.SourceMACAddr.v, sizeof(*remote));

	// Return a simplified version of the EtherType field to the caller
	*type = MAC_UNKNOWN;
	if ((header.Type.v[1] == 0x08u)
			&& ((header.Type.v[0] == ETHER_IP)
					|| (header.Type.v[0] == ETHER_ARP))) {
		*type = header.Type.v[0];
	}

	// Mark this packet as discardable
	WasDiscarded = FALSE;
	return TRUE;
}

/******************************************************************************
 * Function:        void MACPutHeader(MAC_ADDR *remote, BYTE type, WORD dataLen)
 *
 * PreCondition:    MACIsTxReady() must return TRUE.
 *
 * Input:           *remote: Pointer to memory which contains the destination
 *                           MAC address (6 bytes)
 *                  type: The constant ETHER_ARP or ETHER_IP, defining which
 *                        value to write into the Ethernet header's type field.
 *                  dataLen: Length of the Ethernet data payload
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            Because of the dataLen parameter, it is probably
 *                  advantagous to call this function immediately before
 *                  transmitting a packet rather than initially when the
 *                  packet is first created.  The order in which the packet
 *                  is constructed (header first or data first) is not
 *                  important.
 *****************************************************************************/
void MACPutHeader(MAC_ADDR *remote, BYTE type, WORD dataLen) {
	// Set the SPI write pointer to the beginning of the transmit buffer (post per packet control byte)
	WriteReg(EWRPTL, LOW(TXSTART+1));
	WriteReg(EWRPTH, HIGH(TXSTART+1));

	// Calculate where to put the TXND pointer
	dataLen += (WORD) sizeof(ETHER_HEADER) + TXSTART;

	// Write the TXND pointer into the registers, given the dataLen given
	WriteReg(ETXNDL, ((WORD_VAL*) &dataLen)->v[0]);
	WriteReg(ETXNDH, ((WORD_VAL*) &dataLen)->v[1]);

	// Set the per-packet control byte and write the Ethernet destination
	// address
	MACPutArray((BYTE*) remote, sizeof(*remote));

	// Write our MAC address in the Ethernet source field
	MACPutArray((BYTE*) &AppConfig.MyMACAddr, sizeof(AppConfig.MyMACAddr));

	// Write the appropriate Ethernet Type WORD for the protocol being used
	MACPut(0x08);
	MACPut((type == MAC_IP) ? ETHER_IP : ETHER_ARP);
}

/******************************************************************************
 * Function:        void MACFlush(void)
 *
 * PreCondition:    A packet has been created by calling MACPut() and
 *                  MACPutHeader().
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACFlush causes the current TX packet to be sent out on
 *                  the Ethernet medium.  The hardware MAC will take control
 *                  and handle CRC generation, collision retransmission and
 *                  other details.
 *
 * Note:            After transmission completes (MACIsTxReady() returns TRUE),
 *                  the packet can be modified and transmitted again by calling
 *                  MACFlush() again.  Until MACPutHeader() or MACPut() is
 *                  called (in the TX data area), the data in the TX buffer
 *                  will not be corrupted.
 *****************************************************************************/
void MACFlush(void) {
	// Reset transmit logic if a TX Error has previously occured
	// This is a silicon errata workaround
	BFSReg(ECON1, ECON1_TXRST);
	BFCReg(ECON1, ECON1_TXRST);
	BFCReg(EIR, EIR_TXERIF | EIR_TXIF);

	// Start the transmission
	// After transmission completes (MACIsTxReady() returns TRUE), the packet
	// can be modified and transmitted again by calling MACFlush() again.
	// Until MACPutHeader() is called, the data in the TX buffer will not be
	// corrupted.
	BFSReg(ECON1, ECON1_TXRTS);

	// Revision B5 and B7 silicon errata workaround
	if (ENCRevID == 0x05u || ENCRevID == 0x06u) {
		WORD AttemptCounter = 0x0000;
		while (!(ReadETHReg(EIR).Val & (EIR_TXERIF | EIR_TXIF))
				&& (++AttemptCounter < 1000u))
			;
		if (ReadETHReg(EIR).EIRbits.TXERIF || (AttemptCounter >= 1000u)) {
			WORD_VAL ReadPtrSave;
			WORD_VAL TXEnd;
			TXSTATUS TXStatus;
			BYTE i;

			// Cancel the previous transmission if it has become stuck set
			BFCReg(ECON1, ECON1_TXRTS);

			// Save the current read pointer (controlled by application)
			ReadPtrSave.v[0] = ReadETHReg(ERDPTL).Val;
			ReadPtrSave.v[1] = ReadETHReg(ERDPTH).Val;

			// Get the location of the transmit status vector
			TXEnd.v[0] = ReadETHReg(ETXNDL).Val;
			TXEnd.v[1] = ReadETHReg(ETXNDH).Val;
			TXEnd.Val++;

			// Read the transmit status vector
			WriteReg(ERDPTL, TXEnd.v[0]);
			WriteReg(ERDPTH, TXEnd.v[1]);
			MACGetArray((BYTE*) &TXStatus, sizeof(TXStatus));

			// Implement retransmission if a late collision occured (this can
			// happen on B5 when certain link pulses arrive at the same time
			// as the transmission)
			for (i = 0; i < 16u; i++) {
				if (ReadETHReg(EIR).EIRbits.TXERIF
						&& TXStatus.bits.LateCollision) {
					// Reset the TX logic
					BFSReg(ECON1, ECON1_TXRST);
					BFCReg(ECON1, ECON1_TXRST);
					BFCReg(EIR, EIR_TXERIF | EIR_TXIF);

					// Transmit the packet again
					BFSReg(ECON1, ECON1_TXRTS);
					while (!(ReadETHReg(EIR).Val & (EIR_TXERIF | EIR_TXIF)))
						;

					// Cancel the previous transmission if it has become stuck set
					BFCReg(ECON1, ECON1_TXRTS);

					// Read transmit status vector
					WriteReg(ERDPTL, TXEnd.v[0]);
					WriteReg(ERDPTH, TXEnd.v[1]);
					MACGetArray((BYTE*) &TXStatus, sizeof(TXStatus));
				} else {
					break;
				}
			}

			// Restore the current read pointer
			WriteReg(ERDPTL, ReadPtrSave.v[0]);
			WriteReg(ERDPTH, ReadPtrSave.v[1]);
		}
	}
}

/******************************************************************************
 * Function:        void MACSetReadPtrInRx(WORD offset)
 *
 * PreCondition:    A packet has been obtained by calling MACGetHeader() and
 *                  getting a TRUE result.
 *
 * Input:           offset: WORD specifying how many bytes beyond the Ethernet
 *                          header's type field to relocate the SPI read
 *                          pointer.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        SPI read pointer are updated.  All calls to
 *                  MACGet() and MACGetArray() will use these new values.
 *
 * Note:            RXSTOP must be statically defined as being > RXSTART for
 *                  this function to work correctly.  In other words, do not
 *                  define an RX buffer which spans the 0x1FFF->0x0000 memory
 *                  boundary.
 *****************************************************************************/
void MACSetReadPtrInRx(WORD offset) {
	WORD_VAL ReadPT;

	// Determine the address of the beginning of the entire packet
	// and adjust the address to the desired location
	ReadPT.Val = CurrentPacketLocation.Val + sizeof(ENC_PREAMBLE) + offset;

	// Since the receive buffer is circular, adjust if a wraparound is needed
	if (ReadPT.Val > RXSTOP)
		ReadPT.Val -= RXSIZE;

	// Set the SPI read pointer to the new calculated value
	WriteReg(ERDPTL, ReadPT.v[0]);
	WriteReg(ERDPTH, ReadPT.v[1]);
}

/******************************************************************************
 * Function:        PTR_BASE MACSetWritePtr(PTR_BASE Address)
 *
 * PreCondition:    None
 *
 * Input:           Address: Address to seek to
 *
 * Output:          WORD: Old EWRPT location
 *
 * Side Effects:    None
 *
 * Overview:        SPI write pointer is updated.  All calls to
 *                  MACPut() and MACPutArray() will use this new value.
 *
 * Note:            None
 *****************************************************************************/PTR_BASE MACSetWritePtr(
		PTR_BASE address) {
	WORD_VAL oldVal;

	oldVal.v[0] = ReadETHReg(EWRPTL).Val;
	oldVal.v[1] = ReadETHReg(EWRPTH).Val;

	// Set the SPI write pointer to the new calculated value
	WriteReg(EWRPTL, ((WORD_VAL*) &address)->v[0]);
	WriteReg(EWRPTH, ((WORD_VAL*) &address)->v[1]);

	return oldVal.Val;
}

/******************************************************************************
 * Function:        PTR_BASE MACSetReadPtr(PTR_BASE Address)
 *
 * PreCondition:    None
 *
 * Input:           Address: Address to seek to
 *
 * Output:          WORD: Old ERDPT value
 *
 * Side Effects:    None
 *
 * Overview:        SPI write pointer is updated.  All calls to
 *                  MACPut() and MACPutArray() will use this new value.
 *
 * Note:            None
 *****************************************************************************/
 PTR_BASE MACSetReadPtr(PTR_BASE address) {
	WORD_VAL oldVal;

	oldVal.v[0] = ReadETHReg(ERDPTL).Val;
	oldVal.v[1] = ReadETHReg(ERDPTH).Val;

	// Set the SPI write pointer to the new calculated value
	WriteReg(ERDPTL, ((WORD_VAL*) &address)->v[0]);
	WriteReg(ERDPTH, ((WORD_VAL*) &address)->v[1]);

	return oldVal.Val;
}

/******************************************************************************
 * Function:        WORD MACCalcRxChecksum(WORD offset, WORD len)
 *
 * PreCondition:    None
 *
 * Input:           offset  - Number of bytes beyond the beginning of the
 *                          Ethernet data (first byte after the type field)
 *                          where the checksum should begin
 *                  len     - Total number of bytes to include in the checksum
 *
 * Output:          16-bit checksum as defined by RFC 793.
 *
 * Side Effects:    None
 *
 * Overview:        This function performs a checksum calculation in the MAC
 *                  buffer itself
 *
 * Note:            None
 *****************************************************************************/
WORD MACCalcRxChecksum(WORD offset, WORD len) {
	WORD_VAL temp;
	WORD_VAL RDSave;

	// Add the offset requested by firmware plus the Ethernet header
	temp.Val = CurrentPacketLocation.Val + sizeof(ENC_PREAMBLE) + offset;
	if (temp.Val > RXSTOP) // Adjust value if a wrap is needed
	{
		temp.Val -= RXSIZE;
	}

	RDSave.v[0] = ReadETHReg(ERDPTL).Val;
	RDSave.v[1] = ReadETHReg(ERDPTH).Val;

	WriteReg(ERDPTL, temp.v[0]);
	WriteReg(ERDPTH, temp.v[1]);

	temp.Val = CalcIPBufferChecksum(len);

	WriteReg(ERDPTL, RDSave.v[0]);
	WriteReg(ERDPTH, RDSave.v[1]);

	return temp.Val;
}

/******************************************************************************
 * Function:        WORD CalcIPBufferChecksum(WORD len)
 *
 * PreCondition:    Read buffer pointer set to starting of checksum data
 *
 * Input:           len: Total number of bytes to calculate the checksum over.
 *                       The first byte included in the checksum is the byte
 *                       pointed to by ERDPT, which is updated by calls to
 *                       MACSetReadPtr(), MACGet(), MACGetArray(),
 *                       MACGetHeader(), etc.
 *
 * Output:          16-bit checksum as defined by RFC 793
 *
 * Side Effects:    None
 *
 * Overview:        This function performs a checksum calculation in the MAC
 *                  buffer itself.  The ENC28J60 has a hardware DMA module
 *                  which can calculate the checksum faster than software, so
 *                  this function replaces the CaclIPBufferChecksum() function
 *                  defined in the helpers.c file.  Through the use of
 *                  preprocessor defines, this replacement is automatic.
 *
 * Note:            This function works either in the RX buffer area or the TX
 *                  buffer area.  No validation is done on the len parameter.
 *****************************************************************************/
WORD CalcIPBufferChecksum(WORD len) {
	WORD_VAL Start;
	DWORD_VAL Checksum = { 0x00000000ul };
	WORD ChunkLen;
	WORD DataBuffer[10];
	WORD *DataPtr;

	// Save the SPI read pointer starting address
	Start.v[0] = ReadETHReg(ERDPTL).Val;
	Start.v[1] = ReadETHReg(ERDPTH).Val;

	while (len) {
		// Obtain a chunk of data (less SPI overhead compared
		// to requesting one byte at a time)
		ChunkLen = len > sizeof(DataBuffer) ? sizeof(DataBuffer) : len;
		MACGetArray((BYTE*) DataBuffer, ChunkLen);

		len -= ChunkLen;

		// Take care of a last odd numbered data byte
		if (((WORD_VAL*) &ChunkLen)->bits.b0) {
			((BYTE*) DataBuffer)[ChunkLen] = 0x00;
			ChunkLen++;
		}

		// Calculate the checksum over this chunk
		DataPtr = DataBuffer;
		while (ChunkLen) {
			Checksum.Val += *DataPtr++;
			ChunkLen -= 2;
		}
	}

	// Restore old read pointer location
	WriteReg(ERDPTL, Start.v[0]);
	WriteReg(ERDPTH, Start.v[1]);

	// Do an end-around carry (one's complement arrithmatic)
	Checksum.Val = (DWORD) Checksum.w[0] + (DWORD) Checksum.w[1];

	// Do another end-around carry in case if the prior add
	// caused a carry out
	Checksum.w[0] += Checksum.w[1];

	// Return the resulting checksum
	return ~Checksum.w[0];
}

/******************************************************************************
 * Function:        void MACMemCopyAsync(PTR_BASE destAddr, PTR_BASE sourceAddr, WORD len)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           destAddr:   Destination address in the Ethernet memory to
 *                              copy to.  If (PTR_BASE)-1 is specified, the 
 *								current EWRPT value will be used instead.
 *                  sourceAddr: Source address to read from.  If (PTR_BASE)-1 is
 *                              specified, the current ERDPT value will be used
 *                              instead.
 *                  len:        Number of bytes to copy
 *
 * Output:          Byte read from the ENC28J60's RAM
 *
 * Side Effects:    None
 *
 * Overview:        Bytes are asynchronously transfered within the buffer.  Call
 *                  MACIsMemCopyDone() to see when the transfer is complete.
 *
 * Note:            If a prior transfer is already in progress prior to
 *                  calling this function, this function will block until it
 *                  can start this transfer.
 *
 *                  If (PTR_BASE)-1 is used for the sourceAddr or destAddr
 *                  parameters, then that pointer will get updated with the
 *                  next address after the read or write.
 *****************************************************************************/
void MACMemCopyAsync(PTR_BASE destAddr, PTR_BASE sourceAddr, WORD len) {
	WORD_VAL ReadSave, WriteSave;
	BOOL UpdateWritePointer = FALSE;
	BOOL UpdateReadPointer = FALSE;

	if (destAddr == (PTR_BASE) -1) {
		UpdateWritePointer = TRUE;
		destAddr = ReadETHReg(EWRPTL).Val;
		((BYTE*) &destAddr)[1] = ReadETHReg(EWRPTH).Val;
	}
	if (sourceAddr == (PTR_BASE) -1) {
		UpdateReadPointer = TRUE;
		sourceAddr = ReadETHReg(ERDPTL).Val;
		((BYTE*) &sourceAddr)[1] = ReadETHReg(ERDPTH).Val;
	}

	// Handle special conditions where len == 0 or len == 1
	// The DMA module is not capable of handling those corner cases
	if (len <= 1u) {
		if (!UpdateReadPointer) {
			ReadSave.v[0] = ReadETHReg(ERDPTL).Val;
			ReadSave.v[1] = ReadETHReg(ERDPTH).Val;
		}
		if (!UpdateWritePointer) {
			WriteSave.v[0] = ReadETHReg(EWRPTL).Val;
			WriteSave.v[1] = ReadETHReg(EWRPTH).Val;
		}
		WriteReg(ERDPTL, ((BYTE*) &sourceAddr)[0]);
		WriteReg(ERDPTH, ((BYTE*) &sourceAddr)[1]);
		WriteReg(EWRPTL, ((BYTE*) &destAddr)[0]);
		WriteReg(EWRPTH, ((BYTE*) &destAddr)[1]);
		while (len--)
			MACPut(MACGet());
		if (!UpdateReadPointer) {
			WriteReg(ERDPTL, ReadSave.v[0]);
			WriteReg(ERDPTH, ReadSave.v[1]);
		}
		if (!UpdateWritePointer) {
			WriteReg(EWRPTL, WriteSave.v[0]);
			WriteReg(EWRPTH, WriteSave.v[1]);
		}
	} else {
		if (UpdateWritePointer) {
			WriteSave.Val = destAddr + len;
			WriteReg(EWRPTL, WriteSave.v[0]);
			WriteReg(EWRPTH, WriteSave.v[1]);
		}
		len += sourceAddr - 1;
		while (ReadETHReg(ECON1).ECON1bits.DMAST)
			;
		WriteReg(EDMASTL, ((BYTE*) &sourceAddr)[0]);
		WriteReg(EDMASTH, ((BYTE*) &sourceAddr)[1]);
		WriteReg(EDMADSTL, ((BYTE*) &destAddr)[0]);
		WriteReg(EDMADSTH, ((BYTE*) &destAddr)[1]);
		if ((sourceAddr <= RXSTOP) && (len > RXSTOP)) //&& (sourceAddr >= RXSTART))
			len -= RXSIZE;
		WriteReg(EDMANDL, ((BYTE*) &len)[0]);
		WriteReg(EDMANDH, ((BYTE*) &len)[1]);
		BFCReg(ECON1, ECON1_CSUMEN);
		BFSReg(ECON1, ECON1_DMAST);
		if (UpdateReadPointer) {
			len++;
			if ((sourceAddr <= RXSTOP) && (len > RXSTOP)) //&& (sourceAddr >= RXSTART))
				len -= RXSIZE;
			WriteReg(ERDPTL, ((BYTE*) &len)[0]);
			WriteReg(ERDPTH, ((BYTE*) &len)[1]);
		}
	}
}

BOOL MACIsMemCopyDone(void) {
	return !ReadETHReg(ECON1).ECON1bits.DMAST;
}

/******************************************************************************
 * Function:        BYTE MACGet()
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  ERDPT must point to the place to read from.
 *
 * Input:           None
 *
 * Output:          Byte read from the ENC28J60's RAM
 *
 * Side Effects:    None
 *
 * Overview:        MACGet returns the byte pointed to by ERDPT and
 *                  increments ERDPT so MACGet() can be called again.  The
 *                  increment will follow the receive buffer wrapping boundary.
 *
 * Note:            None
 *****************************************************************************/
BYTE MACGet() {
	BYTE data;

	ETH_CS_OUT &= ~ETH_CS;

    // Send the Read Buffer Memory opcode
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = RBM;
    while(!(UCB0IFG & UCRXIFG));
    data = UCB0RXBUF;

    // Send a dummy byte to read the RAM byte pointed by ERDPT
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = 0;
    while(!(UCB0IFG & UCRXIFG));
    data = UCB0RXBUF;

	ETH_CS_OUT |= ETH_CS;

	return data;

} //end MACGet

/******************************************************************************
 * Function:        WORD MACGetArray(BYTE *val, WORD len)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  ERDPT must point to the place to read from.
 *
 * Input:           *val: Pointer to storage location
 *                  len:  Number of bytes to read from the data buffer.
 *
 * Output:          Byte(s) of data read from the data buffer.
 *
 * Side Effects:    None
 *
 * Overview:        Burst reads several sequential bytes from the data buffer
 *                  and places them into local memory.  With SPI burst support,
 *                  it performs much faster than multiple MACGet() calls.
 *                  ERDPT is incremented after each byte, following the same
 *                  rules as MACGet().
 *
 * Note:            None
 *****************************************************************************/
WORD MACGetArray(BYTE *val, WORD len) {
	WORD i = len;
	volatile BYTE Dummy;

	ETH_CS_OUT &= ~ETH_CS;

	// Send the Read Buffer Memory opcode
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = RBM;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

    // Read the amount of bytes specified by len
    while(i--){
        while(!(UCB0IFG & UCTXIFG));
        UCB0TXBUF = 0;
        while(!(UCB0IFG & UCRXIFG));
        *val++ = UCB0RXBUF;
    }

	ETH_CS_OUT |= ETH_CS;

	return len;

} //end MACGetArray

/******************************************************************************
 * Function:        void MACPut(BYTE val)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  EWRPT must point to the location to begin writing.
 *
 * Input:           Byte to write into the ENC28J60 buffer memory
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACPut outputs the Write Buffer Memory opcode/constant
 *                  (8 bits) and data to write (8 bits) over the SPI.
 *                  EWRPT is incremented after the write.
 *
 * Note:            None
 *****************************************************************************/
void MACPut(BYTE val) {
	volatile BYTE Dummy;

	ETH_CS_OUT &= ~ETH_CS;

    // Send the Write Buffer Memory opcode
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = WBM;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

    // Send the byte to be written
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = val;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

	ETH_CS_OUT |= ETH_CS;

} //end MACPut

/******************************************************************************
 * Function:        void MACPutArray(BYTE *val, WORD len)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  EWRPT must point to the location to begin writing.
 *
 * Input:           *val: Pointer to source of bytes to copy.
 *                  len:  Number of bytes to write to the data buffer.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACPutArray writes several sequential bytes to the
 *                  ENC28J60 RAM.  It performs faster than multiple MACPut()
 *                  calls.  EWRPT is incremented by len.
 *
 * Note:            None
 *****************************************************************************/
void MACPutArray(BYTE *val, WORD len) {
	volatile BYTE Dummy;

    ETH_CS_OUT = ~ETH_CS;

    // Send the Write Buffer Memory opcode
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = WBM;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

    // Send the bytes to be written
    while(len){
        while(!(UCB0IFG & UCTXIFG));
        UCB0TXBUF = *val;
        val++;
        len--;
        while(!(UCB0IFG & UCRXIFG));
        Dummy = UCB0RXBUF;
    }

	ETH_CS_OUT |= ETH_CS;
} //end MACPutArray

/******************************************************************************
 * Function:        static void SendSystemReset(void)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        SendSystemReset sends the System Reset SPI command to
 *                  the Ethernet controller.  It resets all register contents
 *                  (except for ECOCON) and returns the device to the power
 *                  on default state.
 *
 * Note:            None
 *****************************************************************************/
static void SendSystemReset(void){
    volatile BYTE Dummy;

    // Note: The power save feature may prevent the reset from executing, so
    // we must make sure that the device is not in power save before issuing
    // a reset.
    BFCReg(ECON2, ECON2_PWRSV);

    // Give some opportunity for the regulator to reach normal regulation and
    // have all clocks running
    _delay_cycles(8000); //-> Delay 1ms with a 8Mhz CPU clock

    // Send the System Reset opcode
    ETH_CS_OUT = ~ETH_CS;
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = SR;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;
    ETH_CS_OUT |= ETH_CS;

    // Wait for the oscillator start up timer and PHY to become ready
    _delay_cycles(8000); //-> Delay 1ms with a 8Mhz CPU clock
}

/******************************************************************************
 * Function:        REG ReadETHReg(BYTE Address)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  Bank select bits must be set corresponding to the register
 *                  to read from.
 *
 * Input:           5 bit address of the ETH control register to read from.
 *                    The top 3 bits must be 0.
 *
 * Output:          Byte read from the Ethernet controller's ETH register.
 *
 * Side Effects:    None
 *
 * Overview:        ReadETHReg sends the 8 bit RCR opcode/Address byte over
 *                  the SPI and then retrives the register contents in the
 *                  next 8 SPI clocks.
 *
 * Note:            This routine cannot be used to access MAC/MII or PHY
 *                  registers.  Use ReadMACReg() or ReadPHYReg() for that
 *                  purpose.
 *****************************************************************************/
REG ReadETHReg(BYTE Address) {
    REG r;

    ETH_CS_OUT = ~ETH_CS;

    // Send the Read Control Register opcode and address
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = RCR | Address;
    while(!(UCB0IFG & UCRXIFG));
    r.Val = UCB0RXBUF;

    // Send a dummy byte to receive the register contents
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = 0;
    while(!(UCB0IFG & UCRXIFG));
    r.Val = UCB0RXBUF;

    ETH_CS_OUT |= ETH_CS;

    return r;

} //end ReadETHReg

/******************************************************************************
 * Function:        REG ReadMACReg(BYTE Address)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  Bank select bits must be set corresponding to the register
 *                  to read from.
 *
 * Input:           5 bit address of the MAC or MII register to read from.
 *                    The top 3 bits must be 0.
 *
 * Output:          Byte read from the Ethernet controller's MAC/MII register.
 *
 * Side Effects:    None
 *
 * Overview:        ReadMACReg sends the 8 bit RCR opcode/Address byte as well
 *                  as a dummy byte over the SPI and then retrieves the
 *                  register contents in the last 8 SPI clocks.
 *
 * Note:            This routine cannot be used to access ETH or PHY
 *                  registers.  Use ReadETHReg() or ReadPHYReg() for that
 *                  purpose.
 *****************************************************************************/
static REG ReadMACReg(BYTE Address) {
    REG r;

    ETH_CS_OUT = ~ETH_CS;

    // Send the Read Control Register opcode and address
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = RCR | Address;
    while(!(UCB0IFG & UCRXIFG));
    r.Val = UCB0RXBUF;

    //Send a dummy byte to receive the dummy byte that the ENC28J60 transmits before a MAC register content
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = 0;
    while(!(UCB0IFG & UCRXIFG));
    r.Val = UCB0RXBUF;

    // Send another dummy byte to receive the real MAC register contents
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = 0;
    while(!(UCB0IFG & UCRXIFG));
    r.Val = UCB0RXBUF;

    ETH_CS_OUT |= ETH_CS;

    return r;

} //end ReadMACReg

/******************************************************************************
 * Function:        ReadPHYReg
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           Address of the PHY register to read from.
 *
 * Output:          16 bits of data read from the PHY register.
 *
 * Side Effects:    None
 *
 * Overview:        ReadPHYReg performs an MII read operation.  While in
 *                  progress, it simply polls the MII BUSY bit wasting time
 *                  (10.24us).
 *
 * Note:            None
 *****************************************************************************/
PHYREG ReadPHYReg(BYTE Register) {
	PHYREG Result;

	// Set the right address and start the register read operation
	BankSel(MIREGADR);
	WriteReg((BYTE) MIREGADR, Register);
	WriteReg((BYTE) MICMD, MICMD_MIIRD);

	// Loop to wait until the PHY register has been read through the MII
	// This requires 10.24us
	BankSel(MISTAT);

	while (ReadMACReg((BYTE) MISTAT).MISTATbits.MISTAT_BUSY_BIT);

	// Stop reading
	BankSel(MIREGADR);
	WriteReg((BYTE) MICMD, 0x00);

	// Obtain results and return
	Result.VAL.v[0] = ReadMACReg((BYTE) MIRDL).Val;
	Result.VAL.v[1] = ReadMACReg((BYTE) MIRDH).Val;

	BankSel(ERDPTL); // Return to Bank 0
	return Result;
} //end ReadPHYReg

/******************************************************************************
 * Function:        void WriteReg(BYTE Address, BYTE Data)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  Bank select bits must be set corresponding to the register
 *                  to modify.
 *
 * Input:           5 bit address of the ETH, MAC, or MII register to modify.
 *                    The top 3 bits must be 0.
 *                  Byte to be written into the register.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        WriteReg sends the 8 bit WCR opcode/Address byte over the
 *                  SPI and then sends the data to write in the next 8 SPI
 *                  clocks.
 *
 * Note:            This routine is almost identical to the BFCReg() and
 *                  BFSReg() functions.  It is seperate to maximize speed.
 *                  Unlike the ReadETHReg/ReadMACReg functions, WriteReg()
 *                  can write to any ETH or MAC register.  Writing to PHY
 *                  registers must be accomplished with WritePHYReg().
 *****************************************************************************/
static void WriteReg(BYTE Address, BYTE Data) {
	volatile BYTE Dummy;

    ETH_CS_OUT = ~ETH_CS;

    // Send the Write Control Register opcode and address
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = WCR | Address;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

    // Send the byte to be written
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = Data;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

    ETH_CS_OUT |= ETH_CS;
} //end WriteReg

/******************************************************************************
 * Function:        void BFCReg(BYTE Address, BYTE Data)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  Bank select bits must be set corresponding to the register
 *                    to modify.
 *
 * Input:           5 bit address of the register to modify.  The top 3 bits
 *                    must be 0.
 *                  Byte to be used with the Bit Field Clear operation.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        BFCReg sends the 8 bit BFC opcode/Address byte over the
 *                  SPI and then sends the data in the next 8 SPI clocks.
 *
 * Note:            This routine is almost identical to the WriteReg() and
 *                  BFSReg() functions.  It is separate to maximize speed.
 *                  BFCReg() must only be used on ETH registers.
 *****************************************************************************/
static void BFCReg(BYTE Address, BYTE Data) {
	volatile BYTE Dummy;

    ETH_CS_OUT = ~ETH_CS;

    // Send the Bit Field Clear opcode and address
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = BFC | Address;
    while(!(UCB0IFG & UCRXIFG));
    Dummy= UCB0RXBUF;

    // Send the byte to be written
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = Data;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

    ETH_CS_OUT |= ETH_CS;
} //end BFCReg

/******************************************************************************
 * Function:        void BFSReg(BYTE Address, BYTE Data)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *                  Bank select bits must be set corresponding to the register
 *                  to modify.
 *
 * Input:           5 bit address of the register to modify.  The top 3 bits
 *                    must be 0.
 *                  Byte to be used with the Bit Field Set operation.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        BFSReg sends the 8 bit BFC opcode/Address byte over the
 *                  SPI and then sends the data in the next 8 SPI clocks.
 *
 * Note:            This routine is almost identical to the WriteReg() and
 *                  BFCReg() functions.  It is separate to maximize speed.
 *                  BFSReg() must only be used on ETH registers.
 *****************************************************************************/
static void BFSReg(BYTE Address, BYTE Data) {
	volatile BYTE Dummy;

    ETH_CS_OUT = ~ETH_CS;

    // Send the Bit Field Set opcode and address
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = BFS | Address;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

    // Send the byte to be written
    while(!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = Data;
    while(!(UCB0IFG & UCRXIFG));
    Dummy = UCB0RXBUF;

    ETH_CS_OUT |= ETH_CS;
} //end BFSReg

/******************************************************************************
 * Function:        WritePHYReg
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           Address of the PHY register to write to.
 *                  16 bits of data to write to PHY register.
 *
 * Output:          None
 *
 * Side Effects:    Alters bank bits to point to Bank 3
 *
 * Overview:        WritePHYReg performs an MII write operation.  While in
 *                  progress, it simply polls the MII BUSY bit wasting time.
 *
 * Note:            None
 *****************************************************************************/
void WritePHYReg(BYTE Register, WORD Data) {
	// Write the register address
	BankSel(MIREGADR);
	WriteReg((BYTE) MIREGADR, Register);

	// Write the data
	// Order is important: write low byte first, high byte last
	WriteReg((BYTE) MIWRL, ((WORD_VAL*) &Data)->v[0]);
	WriteReg((BYTE) MIWRH, ((WORD_VAL*) &Data)->v[1]);

	// Wait until the PHY register has been written
	BankSel(MISTAT);
	while (ReadMACReg((BYTE) MISTAT).MISTATbits.MISTAT_BUSY_BIT);

	BankSel(ERDPTL); // Return to Bank 0
} //end WritePHYReg

/******************************************************************************
 * Function:        BankSel
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           Register address with the high byte containing the 2 bank
 *                    select 2 bits.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        BankSel takes the high byte of a register address and
 *                  changes the bank select bits in ETHCON1 to match.
 *
 * Note:            None
 *****************************************************************************/
void BankSel(WORD Register) {
    BFCReg(ECON1, ECON1_BSEL1 | ECON1_BSEL0);
    BFSReg(ECON1, ((WORD_VAL*)&Register)->v[1]);
} //end BankSel

/******************************************************************************
 * Function:        void MACPowerDown(void)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACPowerDown puts the ENC28J60 in low power sleep mode. In
 *                  sleep mode, no packets can be transmitted or received.
 *                  All MAC and PHY registers should not be accessed.
 *
 * Note:            If a packet is being transmitted while this function is
 *                  called, this function will block until it is it complete.
 *                  If anything is being received, it will be completed.
 *****************************************************************************/
void MACPowerDown(void) {
	// Disable packet reception
	BFCReg(ECON1, ECON1_RXEN);

	// Make sure any last packet which was in-progress when RXEN was cleared
	// is completed
	while (ReadETHReg(ESTAT).ESTATbits.RXBUSY);

	// If a packet is being transmitted, wait for it to finish
	while (ReadETHReg(ECON1).ECON1bits.TXRTS);

	// Enter sleep mode
	BFSReg(ECON2, ECON2_PWRSV);
} //end MACPowerDown

/******************************************************************************
 * Function:        void MACPowerUp(void)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACPowerUp returns the ENC28J60 back to normal operation
 *                  after a previous call to MACPowerDown().  Calling this
 *                  function when already powered up will have no effect.
 *
 * Note:            If a link partner is present, it will take 10s of
 *                  milliseconds before a new link will be established after
 *                  waking up.  While not linked, packets which are
 *                  transmitted will most likely be lost.  MACIsLinked() can
 *                  be called to determine if a link is established.
 *****************************************************************************/
void MACPowerUp(void) {
	// Leave power down mode
	BFCReg(ECON2, ECON2_PWRSV);

	// Wait for the 300us Oscillator Startup Timer (OST) to time out.  This
	// delay is required for the PHY module to return to an operational state.
	while (!ReadETHReg(ESTAT).ESTATbits.CLKRDY);

	// Enable packet reception
	BFSReg(ECON1, ECON1_RXEN);
} //end MACPowerUp

/******************************************************************************
 * Function:        void SetCLKOUT(BYTE NewConfig)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           NewConfig - 0x00: CLKOUT disabled (pin driven low)
 *                              0x01: Divide by 1 (25 MHz)
 *                              0x02: Divide by 2 (12.5 MHz)
 *                              0x03: Divide by 3 (8.333333 MHz)
 *                              0x04: Divide by 4 (6.25 MHz, POR default)
 *                              0x05: Divide by 8 (3.125 MHz)
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Writes the value of NewConfig into the ECOCON register.
 *                  The CLKOUT pin will beginning outputting the new frequency
 *                  immediately.
 *
 * Note:
 *****************************************************************************/
void SetCLKOUT(BYTE NewConfig) {
	BankSel(ECOCON);
	WriteReg((BYTE) ECOCON, NewConfig);
	BankSel(ERDPTL);
} //end SetCLKOUT

/******************************************************************************
 * Function:        BYTE GetCLKOUT(void)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           None
 *
 * Output:          BYTE - 0x00: CLKOUT disabled (pin driven low)
 *                         0x01: Divide by 1 (25 MHz)
 *                         0x02: Divide by 2 (12.5 MHz)
 *                         0x03: Divide by 3 (8.333333 MHz)
 *                         0x04: Divide by 4 (6.25 MHz, POR default)
 *                         0x05: Divide by 8 (3.125 MHz)
 *                         0x06: Reserved
 *                         0x07: Reserved
 *
 * Side Effects:    None
 *
 * Overview:        Returns the current value of the ECOCON register.
 *
 * Note:            None
 *****************************************************************************/
BYTE GetCLKOUT(void) {
	BYTE i;

	BankSel(ECOCON);
	i = ReadETHReg((BYTE) ECOCON).Val;
	BankSel(ERDPTL);
	return i;
} //end GetCLKOUT

#endif //#if defined(ENC_CS_TRIS)
