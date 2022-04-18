/******************************************************************************/
/**
@file		bitarr.c
@author		Ramon Lawrence
@brief		Bit vector implementation
@copyright	Copyright 2022
			The University of British Columbia,		
@par Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

@par 1.Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

@par 2.Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

@par 3.Neither the name of the copyright holder nor the names of its contributors
	may be used to endorse or promote products derived from this software without
	specific prior written permission.

@par THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
/******************************************************************************/

#include "bitarr.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>


/**
@brief     	Initializes bit vector to given size.
@param     	vector
                Bit vector pointer
@param		size
				Size of bit vector
@param		value
				Initialize vector to either 0 or 1.
*/
void bitarrInit(bitarr* vector, uint32_t size, uint8_t value)
{
	uint32_t count = ceil(size / BV_UNIT_SIZE);

	*vector = (bitarr) malloc(BV_UNIT_SIZE * count);
	if (value > 0)
		value = 255;
	for (uint32_t i = 0; i < size; i++)
		(*vector)[i] = value;
}

/**
@brief     	Sets given bit in bit vector.
@param     	vector
                Bit vector pointer
@param		pos
				Location in bit vector indexed from 0.
@param		value
				Either 0 or 1.
*/
void bitarrSet(bitarr vector, uint32_t pos, uint8_t value)
{	
	if (value) 
		vector[pos / BV_UNIT_SIZE] |= (1 << (pos % BV_UNIT_SIZE));
   	else 
      	vector[pos / BV_UNIT_SIZE] &= ~(1 << (pos % BV_UNIT_SIZE));   
}

/**
@brief     	Gets given bit in bit vector.
@param     	vector
                Bit vector pointer
@param		pos
				Location in bit vector indexed from 0.
@return		Bit value either 0 or 1.
*/
uint8_t bitarrGet(bitarr vector, uint32_t pos)
{
	return (vector[pos / BV_UNIT_SIZE] & (1 << (pos % BV_UNIT_SIZE))) > 0 ? 1 : 0;
}

/**
@brief     Prints bit vector contents.
@param     	vector
                Bit vector pointer
@param		size
				Size of bit vector
*/
void bitarrPrint(bitarr vector, uint32_t size)
{
	for (int i=0; i < size; i++)
		printf("%d ", bitarrGet(vector, i));
	printf("\n");
}