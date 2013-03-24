/*
 * Compiler.c
 *
 *  Created on: 30/09/2012
 *      Author: T
 */

#include "Compiler.h"

/*****************************************************************************
  Function:
	WORD swaps(WORD v)

  Description:
	Swaps the endian-ness of a WORD.

  Precondition:
	None

  Parameters:
	v - the WORD to swap

  Returns:
	The swapped version of v.
  ***************************************************************************/
WORD swaps(WORD v)
{
	WORD_VAL t;
	BYTE b;

	t.Val   = v;
	b       = t.v[1];
	t.v[1]  = t.v[0];
	t.v[0]  = b;

	return t.Val;
}


