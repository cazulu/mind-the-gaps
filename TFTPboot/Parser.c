/*
 * Parser.c
 *
 *  Created on: 16/10/2012
 *      Author: T
 */

#include "Parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <msp430.h>
#include "flash.h"

//FLASH and RAM memory ranges
#define RAM_START_ADDR 		(BYTE *)0x001C00
#define RAM_END_ADDR 		(BYTE *)0x005BFF
#define FLASH_START_ADDR	(BYTE *)0x005C00
#define FLASH_END_ADDR 		(BYTE *)0x03FFFF

//File to be translated into machine code and stored in memory
static BYTE * bufferTxtFile;

//Overall status of the parser
static PARSER_STATUS parserStatus;

//This function initializes the parser variables.
//It gets a pointer to the beginning to the TI-TXT file
void ParserInit(BYTE * textBuffer) {
	bufferTxtFile = textBuffer;
	parserStatus = PARSER_NOT_STARTED;
}

//This function converts the TI-TXT format into machine code
//and stores in the addresses specified in it by the @ operator.
//It returns the current status of the parser and gets the amount
//of bytes that the parser is allowed to read.
//NOTE1: This function DOES NOT ERASE the banks where the code
//will be located, so the user has to make sure that all his flash-based
//variables are initialized DURING PROGRAM RUNTIME.
//NOTE2: As the flash self-programming feature works only
//in byte and word mode, this program writes data
//byte by byte.

/* ***Summary of TI-TXT format***
 * @ADDR1
 * DATA01 DATA02 ... DATA16
 * ...
 * DATAm ... DATAn
 * @ADDR2
 * DATA01 ... DATAn
 * ...
 * q
 *
 * -> @ADDR represents the start address of a section
 * -> DATAn represents a byte in hexadecimal, encoded as two chars
 * -> q indicates end of file
 * -> The maximum data bytes per line is 16
 *
 */
PARSER_STATUS ParserTask(int bytesToRead) {
	//Pointer to the current position to write code in
	static BYTE * currentCodePtr;
	//Pointer to the next text char to be parsed
	static char * currentTxtPtr;
	//Buffer to store the last chars received, to avoid
	//the problem of a field split in two TFTP blocks
	static char textBuf[7];
	//Stores the number of newline characters read
	static BYTE nNewlineChar;
	//Stores the number of white space characters read
	static BYTE nWhitespaceChar;
	//Stores the number of address nibbles read after the '@'
	static BYTE nNibble;
	//Indicates if the newlines of the file are Unix-like(\n)
	//or Windows-like(\r\n)
	static BOOL unixNewline=TRUE;
	//Indicates if we are reading the first newline of the file,
	//which we use to determine its newline style
	static BOOL isFirstNewline=TRUE;
	//Indicates if the currentCodePtr points to a flash block
	static BOOL writeFlash=TRUE;
	//Indicates if the parser should exit the while() loop
	static BOOL parserExit=FALSE;
	//Indicates if the current segment has been erased
	static BOOL segmentErased=FALSE;
	//Indicates if there's a need to backup the lower
	//data bytes of the current segment before an erase
	//(maybe they belong to another program's code)
	static BOOL backupNeeded=FALSE;

	static char c;
	static BYTE v, aux;

	// State machine states.
	static enum {
		SM_PARSE_START,
		SM_PARSE_GET_ADDRESS,
		SM_PARSE_GET_BYTE,
		SM_PARSE_WRITE_BYTE,
		SM_PARSE_NEW_LINE,
		SM_PARSE_WHITE_SPACE,
		SM_PARSE_END_OF_CODE,
		SM_PARSE_WRONG_FORMAT
	} smParse = SM_PARSE_START;

	//parserExit should only be set to TRUE inside the state machine loop
	parserExit = FALSE;
	//Each time the ParserTask() is called, we assume that a new 512B
	//TFTP data block has been received and stored in the buffer
	//so we reset the text pointer to the beginning of the buffer.
	currentTxtPtr = (char *) bufferTxtFile;

	while (bytesToRead > 0 && !parserExit) {
		switch (smParse) {

		case SM_PARSE_START:
			//Disable the interrupts, the parser might attempt to modify
			//ISR or interrupt vectors.
			__bic_SR_register(GIE);

			currentTxtPtr = (char*) bufferTxtFile;
			c = *currentTxtPtr++;
			bytesToRead--;
			isFirstNewline = TRUE;
			nNibble = 0;
			nNewlineChar = 0;
			nWhitespaceChar = 0;
			if (c != '@')
				smParse = SM_PARSE_WRONG_FORMAT;
			else {
				smParse = SM_PARSE_GET_ADDRESS;
				parserStatus = PARSER_READING;
			}
			break;

		case SM_PARSE_GET_ADDRESS:
			c = *currentTxtPtr;
			//If the char is an hexadecimal number, we store it in a buffer
			//until we have the full address
			if ((c >= '0' && c < '9') || (c >= 'A' && c <= 'F')
					|| (c >= 'a' && c <= 'f')) {
				textBuf[nNibble] = c;
				currentTxtPtr++;
				bytesToRead--;
				nNibble++;
			}
			//As the addresses can be 16b or 20b,
			//they can contain 4 to 6 nibbles
			else if (nNibble >= 4 && nNibble <= 6) {
				//We store the string terminator
				textBuf[nNibble] = '\0';
				//We read the address, which is stored in hexadecimal format
				currentCodePtr = (BYTE *) strtoul(textBuf, NULL, 16);
				//strtoul returns 0 in case of error, which implies a mistake in the format
				if (currentCodePtr == 0) {
					smParse = SM_PARSE_WRONG_FORMAT;
					break;
				}

				//We check if the pointer belongs to RAM or FLASH and
				//if it does not belong to any of those, we assume a mistake in the format
				if (currentCodePtr >= RAM_START_ADDR
						&& currentCodePtr <= RAM_END_ADDR)
					writeFlash = FALSE;
				else if (currentCodePtr >= FLASH_START_ADDR
						&& currentCodePtr <= FLASH_END_ADDR)
					writeFlash = TRUE;
				else {
					smParse = SM_PARSE_WRONG_FORMAT;
					break;
				}
				smParse = SM_PARSE_NEW_LINE;
				nNibble = 0;
				segmentErased=FALSE;
			}
			//If an unexpected character is read, we consider the format to be wrong
			else {
				smParse = SM_PARSE_WRONG_FORMAT;
				break;
			}
			break;

		case SM_PARSE_NEW_LINE:
			textBuf[nNewlineChar] = *currentTxtPtr++;
			bytesToRead--;

			//We determine the newline style of the file
			if (isFirstNewline) {
				unixNewline = (c == '\n');
				isFirstNewline = FALSE;
			}

			//Newlines are \n in Unix-like system and \r\n in Windows,
			//anything else indicates a wrong format
			if ((unixNewline && nNewlineChar == 1 && textBuf[0] == '\n')
					|| (!unixNewline && nNewlineChar == 2 && textBuf[0] == '\r'
							&& textBuf[1] == '\n')) {
				c = (unixNewline ? textBuf[1] : textBuf[2]);
				if (c == '@')
					smParse = SM_PARSE_GET_ADDRESS;
				else if (c == 'q' || c == 'Q') {
					smParse = SM_PARSE_END_OF_CODE;
				} else {
					smParse = SM_PARSE_GET_BYTE;
					currentTxtPtr--;
					bytesToRead++;
				}
				nNewlineChar = 0;
			} else if ((unixNewline && nNewlineChar >= 1)
					|| (!unixNewline && nNewlineChar >= 2)) {
				smParse = SM_PARSE_WRONG_FORMAT;
			} else {
				nNewlineChar++;
			}
			break;

			//We get a byte that is encoded as an ASCII char
		case SM_PARSE_GET_BYTE:
			c = *currentTxtPtr++;
			bytesToRead--;
			textBuf[nNibble] = c;

			if (c >= '0' && c <= '9')
				textBuf[nNibble] = c - '0';
			else if (c >= 'a' && c <= 'f')
				textBuf[nNibble] = 10 + c - 'a';
			else if (c >= 'A' && c <= 'F')
				textBuf[nNibble] = 10 + c - 'A';
			else {
				smParse = SM_PARSE_WRONG_FORMAT;
				break;
			}

			if (nNibble == 1) {
				v = textBuf[1] + textBuf[0] * 16;
				nNibble = 0;
				smParse = SM_PARSE_WRITE_BYTE;
			} else
				nNibble++;

			break;

			//Each time we read one byte, we write it immediately.
		case SM_PARSE_WRITE_BYTE:
			aux = v;
			//RAM memory can be written directly
			if (!writeFlash){
				*currentCodePtr++ = aux;
				break;
			}

			//We check if the current segment needs an erase
			if(!segmentErased){
				//Pointer to the array of backup bytes for the current segment
				BYTE * segmentBackup = NULL;
				//We set the temp pointer to the beginning of the segment
				BYTE * tempCodePtr = currentCodePtr - ((unsigned long)(currentCodePtr)%512);
				BYTE nBackupBytes=0;

				//If the current code pointer is not at the start of a segment
				//we need to backup the lower data bytes and restore them
				//after the erase
				if(tempCodePtr!=currentCodePtr){
					backupNeeded=TRUE;
					//We reserve memory for the segment backup
					//REMEMBER TO SET A C HEAP SIZE BIG ENOUGH!!!
					segmentBackup = (BYTE *)malloc(currentCodePtr-tempCodePtr);
					//If malloc fails, it returns NULL and we stop the parser
					//TODO: A malloc failure should send the program to different state
					if(segmentBackup==NULL){
						smParse=SM_PARSE_WRONG_FORMAT;
						break;
					}
					//Perform the backup
					for(nBackupBytes=0; tempCodePtr<currentCodePtr; nBackupBytes++){
						segmentBackup[nBackupBytes]=*tempCodePtr++;
					}
				}

				//Erase the segment
				Flash_segmentErase(__MSP430_BASEADDRESS_FLASH__,
											currentCodePtr);
				segmentErased=TRUE;

				//Rewrite the backup data if necessary
				if(backupNeeded){
					//Reset the temp code pointer to the lowest data to be restored
					tempCodePtr -= nBackupBytes;
					Flash_write8(__MSP430_BASEADDRESS_FLASH__, segmentBackup, tempCodePtr,
															nBackupBytes);
					//Free the reserved memory
					free(segmentBackup);
				}

			}

			//Finally, write the byte into flash
			Flash_write8(__MSP430_BASEADDRESS_FLASH__, &aux, currentCodePtr,
					1);
			currentCodePtr++;

			//Check if the pointer is at the beginning of a new segment
			if(((unsigned long)(currentCodePtr)%0x200)==0){
				segmentErased=FALSE;
			}

			//After each byte there should be a white space, according to the TI-TXT format
			smParse = SM_PARSE_WHITE_SPACE;

			break;

		case SM_PARSE_WHITE_SPACE:
			textBuf[nWhitespaceChar] = *currentTxtPtr++;
			bytesToRead--;

			if (nWhitespaceChar == 1 && textBuf[0] == ' ') {
				c = textBuf[1];
				if (c == '\n' || c == '\r')
					smParse = SM_PARSE_NEW_LINE;
				else
					smParse = SM_PARSE_GET_BYTE;
				currentTxtPtr--;
				bytesToRead++;
				nWhitespaceChar = 0;
			} else if (textBuf[0] != ' ') {
				smParse = SM_PARSE_WRONG_FORMAT;
			} else
				nWhitespaceChar++;
			break;

		case SM_PARSE_END_OF_CODE:
			parserStatus = PARSER_END_OF_CODE;
			parserExit = TRUE;
			//Everything went alright, re-enable interrupts
			__bis_SR_register(GIE);
			break;

		//If there's a problem with the format, return FALSE
		case SM_PARSE_WRONG_FORMAT:
			parserStatus = PARSER_ERROR;
			parserExit = TRUE;
			break;

		default:
			smParse = SM_PARSE_START;
			break;
		}
	}

	return parserStatus;
}

