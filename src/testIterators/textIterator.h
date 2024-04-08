/******************************************************************************/
/**
@file		textIterator.h
@author		Ramon Lawrence
@brief		Reads records from a text file as an iterator to provide data to index.
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
#ifndef TEXT_ITERATOR_H
#define TEXT_ITERATOR_H

#include <stdint.h>
#include <stdio.h>

#include "recordIterator.h"

#if defined(ARDUINO)
#include "file/serial_c_iface.h"
#include "file/sdcard_c_iface.h"
#endif

typedef struct textIteratorState 
{	
	recordIteratorState 	state;			/* Basic iterator state */
	#if defined(ARDUINO)
	SD_FILE*				file;			/* Input file */		
	#else
	FILE*					file;			/* Input file */		
	#endif
	char*					filePath;		/* File name with path for input file */
	uint16_t				pageSize;		/* Page size of input file */	
	uint16_t				curRec;			/* Current record index */	
	uint16_t				recordSize;		/* Record size */
	uint8_t					headerRows;		/* header rows to skip */
	char*					separator;		/* Separator for fields */
	uint8_t					keyField;		/* Key field index (from 0) */
	int8_t					dataField;		/* Date field index (from 0). */
											/* If -1, perform secondary index with no data field and record # as 2nd key field. */
} textIteratorState;

/**
@brief     	Returns next record using text file iterator.
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
int8_t textIteratorNext(recordIteratorState *iter, void *key, void *data, uint32_t *recId)
{
	if (iter->nextRecordId >= iter->size)
		return -1;
		
	textIteratorState* it = (textIteratorState*) iter;

	char str[100];

	while (1)
	{
		/* Read row of input */	
		#if defined(ARDUINO)
		if (!sd_fgets(str, 100, it->file))
		#else
		if (!fgets(str, 100, it->file))
		#endif
		{
			printf("Unable to read row in input file.\n");
			iter->size = iter->nextRecordId; 	/* Exhausted all rows in file */
			return -1;
		}		
	   
	   	char *token = strtok(str, it->separator);	
		int8_t idx = 0;
		uint32_t val;
		while (token != NULL) 
		{						
			/* Assumes 32-bit integers for key fields */
			if (idx == it->keyField)
			{
				val = atoi(token);
				memcpy(key, &val, sizeof(uint32_t));
				if (it->dataField == -1)
				{
					memcpy( (void*) ((char*) key + 4), &(iter->nextRecordId), sizeof(uint32_t));  
					break;
				}				
			}
			else if (idx == it->dataField)
			{	/* Assumes 32-bit integers for data fields */
				val = atoi(token);
				memcpy(key, &val, sizeof(uint32_t));
				if (it->keyField < idx)
					break;
			}
			idx++;
			token = strtok(NULL,  it->separator);
		}	
			   
        *recId = iter->nextRecordId;
		it->curRec++;

		iter->nextRecordId++;
		return 0;    
	}                         	                	
}

/**
@brief     	Closes text file iterator.
@param		state
                Record iterator structure
*/
void textIteratorClose(recordIteratorState *iter)
{
	textIteratorState* it = (textIteratorState*) iter;

	fclose(it->file);
	it->file = NULL;
}

/**
@brief     	Initializes text file iterator.
@param		state
                Record iterator structure
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t textIteratorInit(recordIteratorState *iter)
{
	textIteratorState* it = (textIteratorState*) iter;
	char str[200];

	it->state.nextRecordId = 0;
	
	if (it->file == NULL)
	{
		it->file = fopen(it->filePath, "r"); 
		if (it->file == NULL) {
			printf("Error: Can't open file: %s\n", it->filePath);
			return -1;
		}
	}
	else
	{	// File is open. Seek to start of file.	
		fseek(it->file, 0, SEEK_SET); 
	}     	

	/* Read and skip by any header rows */
	for (uint8_t i=0; i < it->headerRows; i++)
	{
		#if defined(ARDUINO)
		if (!sd_fgets(str, 200, it->file))
		#else
		if (!fgets(str, 200, it->file))
		#endif
		{
			printf("Unable to read header row in input file.\n");
			return -1;
		}
		// printf("%s", str);		
	}
	
	it->state.init = textIteratorInit;
	it->state.next = textIteratorNext;
	it->state.close = textIteratorClose;	
	return 0;
}

#endif
