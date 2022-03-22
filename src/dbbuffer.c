/******************************************************************************/
/**
@file		dbbuffer.c
@author		Ramon Lawrence
@brief		Light-weight buffer implementation for small embedded devices.
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
	state->numMoves = 0;
	state->bufferHits = 0;
	state->lastHit = 0;
	state->nextBufferPage = 1;
	// state->endDataPage = state->storage->size;	
	/* Ensure end data page is a multiple of the block size */
	state->endDataPage = (state->storage->size / state->eraseSizeInPages) * state->eraseSizeInPages;
	state->endDataPage--;
	state->storage->size = state->endDataPage;

	/* Set free space flags */
	state->freePages = malloc(sizeof(uint8_t)*(state->storage->size/8+1));
	for (id_t l=0; l < state->storage->size; l++)		
		dbbufferSetFree(state, l);	

	/* Erase first two blocks. */
	erasePages(state, 0, state->eraseSizeInPages*2-1);		
	state->erasedStartPage = 0;
	state->erasedEndPage = state->eraseSizeInPages*2-1;

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
			buf = state->buffer + state->pageSize*2;
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
@brief      Erases physical pages start to end inclusive. Assumes that start and end are aligned according to erase block.
@param     	state
               	DBbuffer state structure
@param     	startPage
                Physical index of start page
@param     	endPage
				Physical index of end page
@return		Return 0 if success, -1 if failure.
*/
int8_t erasePages(dbbuffer *state, id_t startPage, id_t endPage)
{
	// printf("Erasing pages. Start: %d  End: %d\n", startPage, endPage);
	
	state->storage->erasePages(state->storage, startPage, endPage);

	for (id_t l=startPage; l <= endPage; l++)
		dbbufferSetFree(state, l);

	return 0;
}

/**
@brief     	Returns next valid physical page to write.
@param     	state
                DBbuffer state structure		
*/
id_t dbbufferNextValidPage(dbbuffer *state)
{
	/* TODO: Use bit vector instead of looking up mapping again */
	state->nextPageWriteId++;
	while (1)
	{
		if (state->nextPageWriteId > state->endDataPage)		
			state->nextPageWriteId = 0;			
		
		if (dbbufferIsFree(state, state->nextPageWriteId))
		{
			/* Check for mapping. TODO: Should be merged into one step. */
			id_t mapId = vmtreeGetMapping(state->state, state->nextPageWriteId);
			if (mapId == state->nextPageWriteId)
				return state->nextPageWriteId;
			else
			{
				// printf("Page: %lu is not free. Has Mapping: %lu Skipping to next page.\n", state->nextPageWriteId, mapId);
			}	
		}
		state->nextPageWriteId++;
	}	
}

/**
@brief      Writes page to storage. Returns physical page id if success. -1 if failure.
			This version does not check for wrap around.
@param     	state
               	DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@param		pageNum
				Location to write at
@return		
*/
int32_t writePageDirect(dbbuffer *state, void* buffer, int32_t pageNum)
{
	/* Setup page number in header */	
	memcpy(buffer, &(state->nextPageId), sizeof(id_t));
	state->nextPageId++;

	/* Save page in storage */
	state->storage->writePage(state->storage, pageNum, state->pageSize, buffer);
	
	state->numWrites++;
	dbbufferSetValid(state, pageNum);
	return pageNum;	
}

/**
@brief     	Returns 1 if have freed sufficient space up to requested number of pages, 0 otherwise.
			Guarantees that at least that many pages are currently available for writing.
@param     	state
                DBbuffer state structure
@param     	pages
				Number of free pages required				
*/
int8_t dbbufferEnsureSpace(dbbuffer *state, count_t pages)
{
	id_t startErase, endErase;
	id_t parentId = 0;		/* TODO: Not currently used */
	void *parentBuffer;		/* TODO: Not currently used */
	int32_t pageIdToMove[8];		/* TODO: Should be size of erase size */			
	uint8_t numMove;	
	id_t totalPagesLookedAt = 0;

	/* Count how many pages free there are from current write location up to end erase point */
	count_t num=0;
	count_t numCheck;
	id_t page;
	if (state->erasedEndPage >= state->nextPageWriteId)
		numCheck = state->erasedEndPage - state->nextPageWriteId;
	else
		numCheck = state->endDataPage-state->nextPageWriteId + state->erasedEndPage;

	if (numCheck >= 0) // pages)
	{	/* Check that the pages in the range are actually valid */
		page = state->nextPageWriteId;
		for (id_t j=0; j <= numCheck; j++)
		{	if (page > state->endDataPage)
				page = 0;			
			if (dbbufferIsFree(state, page))			
				num++;
			if (num >= pages)
				return 1;
			page++;
		}
	}

	/* Do not have enough free pages. Erase next block */	

finderase:
	numMove = 0;
	startErase = state->erasedEndPage+1;	
	state->erasedStartPage = startErase;
	endErase = startErase + state->eraseSizeInPages - 1;	
	if (endErase > state->endDataPage)
	{	/* Wrap around in memory */
		startErase = 0;
		endErase = state->eraseSizeInPages-1;
	}		

	for (id_t i=startErase; i <= endErase; i++)
	{
		int8_t response = state->isValid(state->state, i, &parentId, &parentBuffer);
		// printf("Status page: %d  Status: %d\n", i, response);
		if (response == -1)
			continue;

		if (response == 1)
		{	/* Mapping but no valid node. Must update parent but not the node itself. */
			/* Active mapping will be detected when try to write page. Page will be skipped. */											
			pageIdToMove[numMove] = -1;		
			// TODO: Need to set dbBufferIsFree to 0 for these pages that are not valid but also cannot overwrite due to mapping?	
			// Works fine now even when set to 1 as dbbufferNextValidPage checks mapping at write time and will not allow write because of that. Consider simplifying.					
		}
		else
		{	/* Valid node at this location. Must rewrite node and its parent. */			
			pageIdToMove[numMove] = i;

			/* Read page */
			void *buf = readPage(state, i);
			if (buf == NULL)
			{
				printf("Read page error: %lu\n", i);
				buf = readPage(state, i);
			}
			/* Copy required pages into buffer block */
			memcpy(state->blockBuffer + numMove * state->pageSize, buf, state->pageSize);
		}
							
		numMove++;
	}

	state->numMoves += numMove;
	
	// if (numMove > 0)
	// 	printf("Number pages moved: %d\n", numMove);

	if (numMove >= state->eraseSizeInPages)			
	{	// Full block. Skip
		// printf("Skipping pages and leaving as is. Start: %d End: %d\n", startErase, endErase);				
		state->erasedEndPage = endErase;
		totalPagesLookedAt += state->eraseSizeInPages;
		// printf("Total pages looked: %lu\n", totalPagesLookedAt);
		if (totalPagesLookedAt >= state->endDataPage - pages)
			return 0;
		goto finderase;
	}



	/* Erase block */
	erasePages(state, startErase, endErase);	
	
	/* Copy pages back into erased block */
	for (id_t i=0; i < numMove; i++)
	{			
		if (pageIdToMove[i] != -1)
		{	/* Must move page */
			// printf("Moving page: %d\n", pageIdToMove[i]);
		
			/* Write page from block buffer */
			writePageDirect(state, state->blockBuffer + i * state->pageSize, pageIdToMove[i]);							
		}
	}

	state->erasedEndPage = endErase;
	
	/* Verify have enough space */
	return dbbufferEnsureSpace(state, pages);
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
	int32_t pageNum = dbbufferNextValidPage(state);

	/* Setup page number in header */	
	memcpy(buffer, &(state->nextPageId), sizeof(id_t));
	state->nextPageId++;
	
	return writePageDirect(state, buffer, pageNum);		
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
	/* Insure all values are 1 in page. */
	/* TODO: May want to initialize to all 1s for certain memory types. */
	/* NOR_OVERWRITE requires everything initialized to 1. */	
	void *buf = state->buffer + pageNum * state->pageSize;
	for (uint16_t i = 0; i < state->pageSize/sizeof(int32_t); i++)
    {
        ((int32_t*) buf)[i] = INT32_MAX;
    }
	state->status[pageNum] = 0;		/* Indicate buffer is unassigned to any current page */
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
	printf("Num moves: %d\n", state->numMoves);
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
	state->numMoves = 0;	
}

/**
@brief     	Set page as free.
@param     	state
                DBbuffer state structure
@param     	pageNum
				Physical index of page				
*/
void dbbufferSetFree(dbbuffer *state, id_t pageNum)
{	
	bitarrSet(state->freePages, pageNum, 1);
	//state->freePages[pageNum] = 1;
	// printf("Freed page: %d\n", pageNum);
}

/**
@brief     	Set page as valid (used).
@param     	state
                DBbuffer state structure
@param     	pageNum
				Physical index of page				
*/
void dbbufferSetValid(dbbuffer *state, id_t pageNum)
{
	bitarrSet(state->freePages, pageNum, 0);
	// state->freePages[pageNum] = 0;
	// printf("Valid page: %d\n", pageNum);
}

/**
@brief     	Returns 1 if page is free (can be used), 0 otherwise.
@param     	state
                DBbuffer state structure
@param     	pageNum
				Physical index of page				
*/
int8_t dbbufferIsFree(dbbuffer *state, id_t pageNum)
{
	// return state->freePages[pageNum];
	return bitarrGet(state->freePages, pageNum);
}