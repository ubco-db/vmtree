/******************************************************************************/
/**
@file		dbbuffer.c
@author		Ramon Lawrence
@brief		Light-weight buffer implementation for small embedded devices.
@copyright	Copyright 2021
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
#include <stdio.h>
#include <string.h>

#include "dbbuffer.h"
#include "vmtree.h"

/**
@brief     	Initializes buffer given page size and number of pages.
@param     	state
                DBbuffer state structure
*/
void dbbufferInit(dbbuffer *state)
{
	printf("Initializing buffer.\n");
	printf("Buffer size: %d  Page size: %d\n", state->numPages, state->pageSize);			
	
	/* TODO: These values would be set during recovery if database already exists. */
	state->nextPageId = 0;
	state->nextPageWriteId = 0;
	
	state->numReads = 0;
	state->numWrites = 0;
	state->numOverWrites = 0;
	state->bufferHits = 0;
	state->lastHit = 0;
	state->nextBufferPage = 1;

	for (count_t l=0; l < state->numPages; l++)
		state->status[l] = 0;	
}


/**
@brief     	Initializes buffer and recovers previous state from storage.
@param     	state
                DBbuffer state structure
*/
void dbbufferRecover(dbbuffer *state)
{
	dbbufferInit(state);

	printf("Recovering from storage.\n");	
	
	/* Scan file from end to determine the page with root */
	// TODO: 
	FILE* fp = NULL; // state->file;
      
	fseek(fp, 0, SEEK_END); 

	uint32_t loc = ftell(fp);

	/* Set next buffer page to write */
	state->nextPageWriteId = loc / state->pageSize;	
	state->nextPageId = state->nextPageWriteId;
	
	for (id_t p = state->nextPageWriteId-1; p >= 0; p--)
	{		
		void *buf = readPage(state, p);
		if (buf == NULL)
			break;
		
		if (VMTREE_IS_ROOT(buf))
		{
			printf("Found root at: %lu\n", p);
			state->activePath[0] = p;
			return;
		}
	}

	printf("Creating new file.\n");

	/* Otherwise assume no root. Create new file. */
	state->nextPageId = 0;
	state->nextPageWriteId = 0;	

	/* Create and write empty root node */	
	void *buf = initBufferPage(state->buffer, 0);	
	VMTREE_SET_ROOT(buf);		
	state->activePath[0] = writePage(state->buffer, buf);		/* Store root location */				
}


/**
@brief      Reads page either from buffer or from storage. Returns pointer to buffer if success.
@param     	state
                DBbuffer state structure
@param     	pageNum
                Physical page id (number)
@return		Returns pointer to buffer page or NULL if error.
*/
void* readPage(dbbuffer *state, id_t pageNum)
{    
	void *buf;
	count_t i;

	/* Check to see if page is currently in buffer */
	for (i=1; i < state->numPages; i++)
	{
		if (state->status[i] == pageNum && pageNum != 0)
		{
			state->bufferHits++;
			buf = state->buffer + state->pageSize*i;
			state->lastHit = state->status[i];
			return buf;
		}
	}

	if (state->numPages == 2)
	{	buf = state->buffer + state->pageSize;
		i = 1;
	}
	else
	{	
		/* Reserve page #1 for root if have at least 3 buffers. */
		if (state->activePath[0] == pageNum)
		{	/* Request for root. */			
			i = 1;
		}
		else if (state->numPages == 3)
		{
			i = 2;
		}
		else
		{
			/* More than minimum pages. Some basic memory management using round robin buffer. */		
			buf = NULL;
		
			/* Determine buffer location for page */
			/* TODO: This needs to be improved and may also consider locking pages */
			for (i=2; i < state->numPages; i++)
			{
				if (state->status[i] == 0)	/* Empty page */
				{	buf = state->buffer + state->pageSize*i;			
					break;
				}
			}

			/* Pick the next page */
			if (buf == NULL)
			{
				i = state->nextBufferPage;
				state->nextBufferPage++;
				
				while (1)
				{
					if (i > state->numPages-1)
					{	i = 2;
						state->nextBufferPage = 2;
					}

					if (state->status[i] != state->lastHit)						
						break;					

					i++;					
				}		
			}
		}
	}
	    
	state->status[i] = pageNum;
	return readPageBuffer(state, pageNum, i);
}

/**
@brief      Reads page to a particular buffer number. Returns pointer to buffer if success.
@param     	state
                DBbuffer state structure
@param     	pageNum
                Physical page id (number)
@param		bufferNum
				Buffer to read into
@return		Returns pointer to buffer page or NULL if error.
*/
void* readPageBuffer(dbbuffer *state, id_t pageNum, count_t bufferNum)
{
	void *buf = state->buffer + bufferNum * state->pageSize;	

	state->storage->readPage(state->storage, pageNum, state->pageSize, buf);
	
    state->numReads++;
	   
	return buf;
}


/**
@brief      Writes page to storage. Returns physical page id if success. -1 if failure.
@param     	state
               	DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@return		
*/
int32_t writePage(dbbuffer *state, void* buffer)
{    	
	/* Always writes to next page number. Returned to user. */	
	int32_t pageNum = state->nextPageWriteId++;	

	/* Setup page number in header */	
	memcpy(buffer, &(state->nextPageId), sizeof(id_t));
	state->nextPageId++;
	
	/* Save page in storage */
	state->storage->writePage(state->storage, pageNum, state->pageSize, buffer);
	
	state->numWrites++;
	return pageNum;
}

/**
@brief      Overwrites page to storage at same physical address. -1 if failure.
			Caller is responsible for knowing that overwrite is possible given page contents.
@param     	state
                DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@return		
*/
int32_t overWritePage(dbbuffer *state, void* buffer, int32_t pageNum)
{			
	state->storage->writePage(state->storage, pageNum, state->pageSize, buffer);
		
	state->numOverWrites++;		
	
	/* Check if buffer contains this page */
	for (count_t i=1; i < state->numPages; i++)
	{				
		if (state->status[i] == pageNum && pageNum != 0)
		{	/* Copy over page */
			if (state->buffer + i*state->pageSize == buffer)
				break;
			
			memcpy(state->buffer + i*state->pageSize, buffer, state->pageSize);
			/* Other choice is to clear the buffer: state->status[i] = 0; */
			break;
		}
	}

	// printf("\nWrite page: %d Id: %d Key: %d\n", pageNum, (state->nextPageId-1), *((int32_t*) (buffer+10)));
	return pageNum;
}


/**
@brief     	Initialize in-memory buffer page.
@param     	state
                DBbuffer state structure
@param     	pageNum
                In memory buffer page id (number)
@return		pointer to initialized page
*/
void* initBufferPage(dbbuffer *state, int pageNum)
{	
	/* Insure all values are 0 in page. */
	/* TODO: May want to initialize to all 1s for certian memory types. */	
	void *buf = state->buffer + pageNum * state->pageSize;
	for (uint16_t i = 0; i < state->pageSize/sizeof(int32_t); i++)
    {
        ((int32_t*) buf)[i] = 0;
    }
	return buf;		
}

/**
@brief     	Closes buffer.
@param     	state
                DBbuffer state structure
*/
void closeBuffer(dbbuffer *state)
{
	printStats(state);	
	state->storage->close(state->storage);	
}


/**
@brief     	Prints statistics.
@param     	state
                DBbuffer state structure
*/
void printStats(dbbuffer *state)
{
	printf("Num reads: %lu\n", state->numReads);
	printf("Buffer hits: %lu\n", state->bufferHits);
	printf("Num writes: %lu\n", state->numWrites);
	printf("Num overwrites: %lu\n", state->numOverWrites);	
}


/**
@brief     	Clears statistics.
@param     	state
                DBbuffer state structure
*/
void dbbufferClearStats(dbbuffer *state)
{
	state->numReads = 0;
	state->numWrites = 0;
	state->bufferHits = 0;
	state->numOverWrites = 0;	
}
