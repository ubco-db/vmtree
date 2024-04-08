/******************************************************************************/
/**
@file		testIterators.h
@author		Ramon Lawrence
@brief		File iterators used for testing and benchmarking.
@copyright	Copyright 2024
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
#ifndef TESTITERATORS_H
#define TESTITERATORS_H

#include "testIterators/randomIterator.h"
#include "testIterators/fileIterator.h"
#include "testIterators/textIterator.h"

/**
 * Random iterator with given record numbers of records.
*/
recordIteratorState* randomIterator(int32_t numRecords)
{
    randomIteratorState* iter = (randomIteratorState*) malloc(sizeof(randomIteratorState));
    randomIteratorInit((recordIteratorState*) iter);

    iter->state.size = numRecords;
    return (recordIteratorState*) iter;
}

/*
Iterates through a binary file of records on storage.
*/
recordIteratorState* fileIterator(int32_t numRecords, char* fileName, uint8_t keyOffset, uint8_t recordSize)
{                            
    fileIteratorState* iter = (fileIteratorState*) malloc(sizeof(fileIteratorState));      
    iter->filePath = fileName;  
    iter->file = NULL;        
    iter->pageSize = 512;
    iter->recordSize = recordSize;
    iter->headerSize = 16;        
    iter->keyOffset = keyOffset;
    iter->buffer = (char*) malloc(iter->pageSize);
    fileIteratorInit((recordIteratorState*) iter);

    iter->state.size = numRecords;
    return (recordIteratorState*) iter;
}
 
/*
Iterates through a text file of records on storage.
*/
recordIteratorState* textIterator(int32_t numRecords, char* fileName, uint8_t headerRows, char* separator, uint8_t keyIdx, int8_t dataIdx)
{                            
    textIteratorState* iter = (textIteratorState*) malloc(sizeof(textIteratorState));      
    iter->filePath = fileName;  
    iter->file = NULL;            
    iter->recordSize = 16;    
    iter->headerRows = headerRows;
    iter->separator = separator;
    iter->keyField = keyIdx;
    iter->dataField = dataIdx;

    if (textIteratorInit((recordIteratorState*) iter) == -1)
      return NULL;

    iter->state.size = numRecords;

    return (recordIteratorState*) iter;
}

#endif
