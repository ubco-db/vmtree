/******************************************************************************/
/**
@file		fileIterator.h
@author		Ramon Lawrence
@brief		Reads records from a file as an iterator to provide data to index.
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
#ifndef FILE_ITERATOR_H
#define FILE_ITERATOR_H

#include <stdint.h>
#include <stdio.h>

#include "recordIterator.h"

#if defined(ARDUINO)
#include "file/serial_c_iface.h"
#include "file/sdcard_c_iface.h"
#endif

typedef struct fileIteratorState 
{	
	recordIteratorState 	state;			/* Basic iterator state */
	SD_FILE*				file;			/* Input file */	
	char*					buffer;			/* Buffer for one page of data file */	
	char*					filePath;		/* File name with path for input file */
	uint16_t				pageSize;		/* Page size of input file */	
	uint16_t				curRec;			/* Current record index */
	uint8_t					headerSize;		/* Page header size */
	uint16_t				recordSize;		/* Record size */
	uint8_t					keyOffset;		/* Offset from start of record for key */
} fileIteratorState;

/**
@brief     	Returns next record using file iterator.
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
int8_t fileIteratorNext(recordIteratorState *iter, void *key, void *data, uint32_t *recId)
{
	if (iter->nextRecordId >= iter->size)
		return -1;
		
	fileIteratorState* it = (fileIteratorState*) iter;

	while (1)
	{
		/* Check to see if have processed all records on page */
		int16_t count = *((int16_t*) (it->buffer+4));           

		if (it->curRec >= count)
		{	/* Read next page */
			if (!fread(it->buffer, it->pageSize, 1, it->file)) 
				return -1;	
			it->curRec = 0;		
		}       

		void *loc = (it->buffer + it->headerSize + it->curRec*it->recordSize);				
        
		/* Secondary index record (dataValue, recordNum) into B-tree secondary index */
		memcpy(key, (void*) ((char*) loc + it->keyOffset), sizeof(uint32_t));
		memcpy( (void*) ((char*) key + 4), &(iter->nextRecordId), sizeof(uint32_t));    
			   
        *recId = iter->nextRecordId;
		it->curRec++;

		iter->nextRecordId++;
		return 0;                             
	}                	
}

/**
@brief     	Closes file iterator.
@param		state
                Record iterator structure
*/
void fileIteratorClose(recordIteratorState *iter)
{
	fileIteratorState* it = (fileIteratorState*) iter;

	fclose(it->file);
	it->file = NULL;
}

/**
@brief     	Initializes file iterator.
@param		state
                Record iterator structure
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t fileIteratorInit(recordIteratorState *iter)
{
	fileIteratorState* it = (fileIteratorState*) iter;
	
	it->state.nextRecordId = 0;
	
	if (it->file == NULL)
	{
		it->file = fopen(it->filePath, "r+b"); 
		if (it->file == NULL) {
			printf("Error: Can't open file: %s\n", it->filePath);
			return -1;
		}
	}
	else
	{	// File is open. Seek to start of file.	
		fseek(it->file, 0, SEEK_SET); 
	}     	

	/* Read and buffer first page */
	if (fread(it->buffer, it->pageSize, 1, it->file) == 0) {
		printf("Unable to read first page in input file.\n");
		return -1;
	}     	

	it->curRec = 0;

	it->state.init = fileIteratorInit;
	it->state.next = fileIteratorNext;
	it->state.close = fileIteratorClose;	
	return 0;
}

#endif
