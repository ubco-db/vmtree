/******************************************************************************/
/**
@file		randomIterator.h
@author		Ramon Lawrence
@brief		Rrandom data record iterator to provide data to index.
@copyright	Copyright 2022
			The University of British Columbia,
			Ramon Lawrence		
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
#ifndef RANDOM_ITERATOR_H
#define RANDOM_ITERATOR_H

#include <stdint.h>

#include "randomseq.h"
#include "recordIterator.h"


typedef struct randomIteratorState 
{	
	recordIteratorState 	state;			/* Basic iterator state */
	randomseqState			seqState;		/* Random sequence state */		
} randomIteratorState;

/**
@brief     	Returns next record using random iterator.
@param		state
                Record iterator structure
@param		key
				Space must be pre-allocated for key.
@param		data
				Space must be pre-allocated for data value.
@param		recId
				Returns record id.
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t randomIteratorNext(recordIteratorState *iter, void *key, void *data, uint32_t *recId)
{
	if (iter->nextRecordId >= iter->size)
		return -1;
		
	randomIteratorState* it = (randomIteratorState*) iter;

	uint32_t v = randomseqNext(&(it->seqState));          
	memcpy(key, &v, sizeof(uint32_t));
	memcpy(data, &v, sizeof(uint32_t));
	*recId = iter->nextRecordId;

	iter->nextRecordId++;
	return 0;
}

/**
@brief     	Closes random iterator.
@param		state
                Record iterator structure
*/
void randomIteratorClose(recordIteratorState *iter)
{
	/* Nothing to do */
}

/**
@brief     	Initializes random iterator.
@param		state
                Record iterator structure
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t randomIteratorInit(recordIteratorState *iter)
{
	randomIteratorState* it = (randomIteratorState*) iter;
	
	it->state.nextRecordId = 0;
	it->seqState.size = it->state.size;
	it->seqState.prime = 0;	
	randomseqInit(&(it->seqState));  
	
	it->state.init = randomIteratorInit;
	it->state.next = randomIteratorNext;
	it->state.close = randomIteratorClose;	

	return 0;
}

#endif
