/*
 * Parser.h
 *
 *  Created on: 16/10/2012
 *      Author: T
 */

#ifndef PARSER_H_
#define PARSER_H_

#include "GenericTypeDefs.h"

// State of the parser to be reported
// to external applications
typedef enum{
	PARSER_NOT_STARTED,		//The parser has not started running
	PARSER_READING,			//The parser is processing the TI-TXT code file
	PARSER_ERROR,			//Error during parsing, the parser will abort
	PARSER_END_OF_CODE		//End of code reached
}PARSER_STATUS;

void ParserInit(BYTE * textBuffer);

//This function converts the TI-TXT format into machine code
//and stores in the addresses specified in it by the @ operator.
//It returns the current status of the parser and gets the amount
//of bytes that the parser is allowed to read.
//NOTE1: This functions DOES NOT ERASE the banks where the code
//will be located, so the user has to make sure that all his
//variables are initialized DURING PROGRAM RUNTIME.
//NOTE2: As the flash self-programming feature works only
//in byte and word mode, this program writes data
//byte by byte.
PARSER_STATUS ParserTask(int bytesToRead);



#endif /* PARSER_H_ */
