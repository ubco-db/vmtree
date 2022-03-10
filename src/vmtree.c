/******************************************************************************/
/**
@file		vmtree.c
@author		Ramon Lawrence
@brief		Implementation for virtual mapping B-tree.
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "vmtree.h"


/*
Comparison functions. Code is adapted from ldbm.
*/
/**
@brief     	Compares two unsigned int32_t values.
@param     	a
                value 1
@param     	b
                value 2
*/
static int8_t uint32Compare(void *a, void *b)
{	
	uint32_t i1, i2;
    memcpy(&i1, a, sizeof(uint32_t));
    memcpy(&i2, b, sizeof(uint32_t));

	if (i1 > i2)
		return 1;
	if (i1 < i2)
		return -1;
	return 0;	
}

/**
@brief     	Compares two values by bytes. 
@param     	a
                value 1
@param     	b
                value 2
*/
static int8_t byteCompare(void *a, void *b, int16_t size)
{
	return memcmp(a, b, size);	
}


/**
@brief     	Initialize a VMTree structure.
@param     	state
                vmTree algorithm state structure
*/
void vmtreeInit(vmtreeState *state)
{
	printf("Initializing VMTree.\n");
	printf("Buffer size: %d  Page size: %d\n", state->buffer->numPages, state->buffer->pageSize);	
	state->recordSize = state->keySize + state->dataSize;
	printf("Record size: %d\n", state->recordSize);	
	
	dbbufferInit(state->buffer);

	state->compareKey = uint32Compare;

	/* Calculate block header size */
	if (state->parameters != NOR_OVERWRITE)
	{
		/* Header size fixed: 10 bytes: 4 byte id, 4 byte prev index, 2 for record count. */	
		state->headerSize = 10;
		state->interiorHeaderSize = state->headerSize;

		/* Calculate number of records per page */
		state->maxRecordsPerPage = (state->buffer->pageSize - state->headerSize) / state->recordSize;
		/* Interior records consist of key and id reference. Note: One extra id reference (child pointer). If N keys, have N+1 id references (pointers). */
		state->maxInteriorRecordsPerPage = (state->buffer->pageSize - state->headerSize - sizeof(id_t)) / (state->keySize+sizeof(id_t));

		printf("Max records per page: %d Interior: %d\n", state->maxRecordsPerPage, state->maxInteriorRecordsPerPage);
	}
	else
	{	/* NOR OVERWRITE has different page structure to allow in-page overwrites. Keys are NOT sorted. */
		/* Header size fixed: 10 bytes: 4 byte id, 4 byte prev index, 2 for record count, +1 minimum for each bitmap. */	
		state->headerSize = 12;

		/* Calculate number of records per page */
		state->maxRecordsPerPage = (state->buffer->pageSize - state->headerSize)*8 / (state->recordSize*8+2);	/* +2 as two status bits per record */
		state->bitmapSize = ceil(state->maxRecordsPerPage / 8.0);
		state->headerSize = 10 + 2 * state->bitmapSize;
		/* Interior records consist of key and id reference. Note: One extra id reference (child pointer). If N keys, have N+1 id references (pointers). */
		state->maxInteriorRecordsPerPage = (state->buffer->pageSize - state->headerSize - sizeof(id_t))*8 / ((state->keySize+sizeof(id_t))*8+2);
		state->interiorBitmapSize = ceil(state->maxInteriorRecordsPerPage / 8.0);
		state->interiorHeaderSize = 10 + 2 * state->interiorBitmapSize;

		printf("Data pages: Max records: %d Header size: %d Bitmap size: %d  Interior pages: Max records: %d Header size: %d Bitmap size: %d\n", 
				state->maxRecordsPerPage, state->headerSize, state->bitmapSize, state->maxInteriorRecordsPerPage, state->interiorHeaderSize, state->interiorBitmapSize);
	}

	/* Hard-code for testing */
	// state->maxRecordsPerPage = 5;	
	// state->maxInteriorRecordsPerPage = 4;	

	state->levels = 1;
	state->numMappings = 0;
	state->numNodes = 1;
	state->numMappingCompare = 0;
	state->maxTries = 1;	
	state->savedMappingPrev = EMPTY_MAPPING;

	/* Calculate maximum number of mappings */
	state->maxMappings = state->mappingBufferSize / (2*sizeof(id_t));		/* TODO: May want to add overhead to this calculation */
	
	printf("Max mappings: %d  Number of hash probes: %d\n", state->maxMappings, state->maxTries);

	/* Initialize mapping table */
	vmtreemapping *mappings = (vmtreemapping*) state->mappingBuffer;
	for (int16_t i=0; i < state->maxMappings; i++)
		mappings[i].prevPage = EMPTY_MAPPING;

	/* Create and write empty root node */
	void *buf = initBufferPage(state->buffer, 0);
	VMTREE_SET_ROOT(buf);
	if (state->parameters != NOR_OVERWRITE)
		VMTREE_SET_COUNT(buf, 0);		
	state->activePath[0] = writePage(state->buffer, buf);		/* Store root location */		
}

/**
@brief     	Return the smallest key in the node
@param     	state
                VMTree algorithm state structure
@param     	buffer
                In memory page buffer with node data
*/
void* vmtreeGetMinKey(vmtreeState *state, void *buffer)
{
	if (state->parameters != NOR_OVERWRITE)
		return (void*) (buffer+state->headerSize);
	else
	{	/* Must search through all values to find minimum key as not sorted. */
		for (int i=0; i < state->maxRecordsPerPage; i++)
		{
			// TODO:
		}
		return NULL;
	}
}

/**
@brief     	Return the smallest key in the node
@param     	state
                VMTree algorithm state structure
@param     	buffer
                In memory page buffer with node data
*/
void* vmtreeGetMaxKey(vmtreeState *state, void *buffer)
{
	if (state->parameters != NOR_OVERWRITE)
	{
		int16_t count =  VMTREE_GET_COUNT(buffer); 
		if (count == 0)
			count = 1;		/* Force to have value in buffer. May not make sense but likely initialized to 0. */
		return (void*) (buffer+state->headerSize+(count-1)*state->recordSize);
	}
	else
	{
		return NULL;
	}
}

/**
@brief     	Gets a page mapping index or returns -1 if no mapping. 
@param     	state
                VMTree algorithm state structure
@param		pageId
				physical page index
*/
int16_t vmtreeGetMappingIndex(vmtreeState *state, id_t pageId)
{		
	if (state->numMappings == 0)
		return -1;

	// Code for fixed sized hash
	vmtreemapping *mappings = (vmtreemapping*) state->mappingBuffer;	
	int16_t loc = pageId % state->maxMappings;
	int8_t i=0;

	while (1)
	{	
		state->numMappingCompare++;
		if (mappings[loc].prevPage == pageId)
			return loc;	 	

		i++;
		if (i >= state->maxTries)
			break;
		loc += 7;
		loc = loc % state->maxMappings;	
	}
	
	return -1;
}

/**
@brief     	Adds a page mapping.
@param     	state
                VMTree algorithm state structure
@param		prevPage
				previous physical page index
@param		currPage
				current physical page index
*/
int8_t vmtreeAddMapping(vmtreeState *state, id_t prevPage, id_t currPage)
{
	/* Check for capacity */
	//if (state->numMappings >= state->maxMappings)
	//	return -1;		/* No space for another mapping */

	vmtreemapping *mappings = (vmtreemapping*) state->mappingBuffer;
	
	int16_t loc = prevPage % state->maxMappings;
	int8_t i=0;
	
	while (1)
	{
		state->numMappingCompare++;
		if (mappings[loc].prevPage == prevPage)
		{	/* Update current mapping */
			mappings[loc].currPage = currPage;
			return 0;	
		}			

 		if (mappings[loc].prevPage == EMPTY_MAPPING)
		{	/* Add new mapping */
			state->numMappings++;
			mappings[loc].prevPage = prevPage;
			mappings[loc].currPage = currPage;
			return 0;	
		}		 	

		i++;
		if (i >= state->maxTries)
			break;

		loc += 7;
		loc = loc % state->maxMappings;	
	}

	/* No space for mapping in mapping chain */
	return -1;	
}

/**
@brief     	Deletes a page mapping.
@param     	state
                VMTree algorithm state structure
@param		prevPage
				previous physical page index
*/
int8_t vmtreeDeleteMapping(vmtreeState *state, id_t prevPage)
{	
	if (state->numMappings == 0)
		return 0;

	vmtreemapping *mappings = (vmtreemapping*) state->mappingBuffer;
			
	int16_t loc = prevPage % state->maxMappings;	
	int8_t i;
	for (i=0; i < state->maxTries; i++)	
	{	
		state->numMappingCompare++;
		if (mappings[loc].prevPage == prevPage)
		{
			mappings[loc].prevPage = EMPTY_MAPPING;	
			state->numMappings--;
			return 0;
		}

		loc += 7;
		loc = loc % state->maxMappings;	
	}

	return 0;
}

/**
@brief     	Gets a page mapping or returns current page number if no mapping.
@param     	state
                VMTree algorithm state structure
@param		pageId
				physical page index
*/
id_t vmtreeGetMapping(vmtreeState *state, id_t pageId)
{	
	vmtreemapping *mappings = (vmtreemapping*) state->mappingBuffer;

	int16_t mappingIdx = vmtreeGetMappingIndex(state, pageId);	
	if (mappingIdx != -1)
		return mappings[mappingIdx].currPage;	
	
	/* Return original page if no mapping */
	return pageId;
}

void printSpaces(int num)
{
	for (int i=0; i < num; i++)
		printf(" ");
}

/**
@brief     	Print a node in an in-memory buffer.
@param     	state
                VMTree algorithm state structure
@param     	pageNum
                Physical page id (number)	
@param     	depth
                Used for nesting print out
@param     	buffer
                In memory page buffer with node data
*/
void vmtreePrintNodeBuffer(vmtreeState *state, id_t pageNum, int depth, void *buffer)
{
	uint32_t key, val;	

	if (state->parameters != NOR_OVERWRITE)
	{	
		int16_t c, count =  VMTREE_GET_COUNT(buffer); 
		if (VMTREE_IS_INTERIOR(buffer) && state->levels != 1)
		{	
			printSpaces(depth*3);
			printf("Id: %d Loc: %d Prev: %d Cnt: %d [%d, %d]\n", VMTREE_GET_ID(buffer), pageNum, VMTREE_GET_PREV(buffer), count, (VMTREE_IS_ROOT(buffer)), VMTREE_IS_INTERIOR(buffer));		
			/* Print data records (optional) */	
			printSpaces(depth*3);
					
			for (c=0; c < count && c < state->maxInteriorRecordsPerPage; c++)
			{			
				memcpy(&key, (int32_t*) (buffer+state->keySize * c + state->headerSize), sizeof(int32_t));
				memcpy(&val, (int32_t*) (buffer+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t)), sizeof(int32_t));
				id_t mapVal = vmtreeGetMapping(state, val);			
				printf(" (%d, %d", key, val);			
				if (mapVal != val)
					printf(" [%d]", mapVal);	
				printf(")");
			}
			
			/* Print last pointer */
			memcpy(&val, (int32_t*) (buffer+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t)), sizeof(uint32_t));
			id_t mapVal = vmtreeGetMapping(state, val);	
			printf(" (, %d", val);
			if (mapVal != val)
				printf(" [%d]", mapVal);
			printf(")\n");
		}
		else
		{	
			printSpaces(depth*3);
			uint32_t minkey, maxkey;
			memcpy(&minkey, (int32_t*) vmtreeGetMinKey(state, buffer), sizeof(uint32_t));
			memcpy(&maxkey, (int32_t*) vmtreeGetMaxKey(state, buffer), sizeof(uint32_t));			
			printf("Id: %lu Loc: %lu Cnt: %d (%lu, %lu)\n", VMTREE_GET_ID(buffer), pageNum, count, minkey, maxkey);

			/* Print data records (optional) */			
			for (int c=0; c < count; c++)
			{				
				memcpy(&key, (int32_t*) (buffer + state->headerSize + state->recordSize * c), sizeof(int32_t));
				memcpy(&val, (int32_t*) (buffer + state->headerSize + state->recordSize * c + state->keySize), sizeof(int32_t));
				printSpaces(depth*3+2);
				printf("Key: %lu Value: %lu\n",key, val);									
			}				
		}
	}
	else
	{
		int16_t c, count = 0;
		if (VMTREE_IS_INTERIOR(buffer) && state->levels != 1)
		{				
			unsigned char* bm1 = buffer + state->interiorHeaderSize - state->interiorBitmapSize*2;
			unsigned char* bm2 = buffer + state->interiorHeaderSize - state->interiorBitmapSize;			

			/* Determine count */
			for (c=0; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
			{	
				if (bitarrGet(bm1, c) == 0 && bitarrGet(bm2, c) == 1)		
					count++;					
			}	
			
			printSpaces(depth*3);
			printf("Id: %d Loc: %d Prev: %d Cnt: %d [%d, %d]\n", VMTREE_GET_ID(buffer), pageNum, VMTREE_GET_PREV(buffer), count, (VMTREE_IS_ROOT(buffer)), VMTREE_IS_INTERIOR(buffer));		
		
			// bitarrPrint(bm1, state->interiorBitmapSize);
			// bitarrPrint(bm2, state->interiorBitmapSize);
			/* Print data records (optional) */	
			printSpaces(depth*3);

			for (c=0; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
			{	/* Must be non-free location (0) and still valid (1) */
				if (bitarrGet(bm1, c) == 0 && bitarrGet(bm2, c) == 1)		
				{
					memcpy(&key, (int32_t*) (buffer+state->keySize * c + state->interiorHeaderSize), sizeof(int32_t));
					memcpy(&val, (int32_t*) (buffer+state->keySize * state->maxInteriorRecordsPerPage + state->interiorHeaderSize + c*sizeof(id_t)), sizeof(id_t));					
					printf(" (%d, %u)", key, val);																	
				}
			}			
			printf("\n");			
		}
		else
		{		
			unsigned char* bm1 = buffer + state->headerSize - state->bitmapSize*2;
			unsigned char* bm2 = buffer + state->headerSize - state->bitmapSize;

			int32_t minKey = INT32_MAX, maxKey = 0;	
			bitarrPrint(bm1, state->bitmapSize);
			bitarrPrint(bm2, state->bitmapSize);		
			for (c=0; c < state->bitmapSize*8 && c < state->maxRecordsPerPage; c++)
			{	/* Must be non-free location (0) and still valid (1) */
				if (bitarrGet(bm1, c) == 0 && bitarrGet(bm2, c) == 1)
				{	/* Found valid record. */					
					
					memcpy(&key, (int32_t*) (buffer+state->keySize * c + state->headerSize), sizeof(int32_t));
					// int32_t val = *((int32_t*) (buffer+state->keySize * state->maxRecordsPerPage + state->headerSize + c*state->dataSize));
				
					if (key < minKey)
						minKey = key;
					if (key > maxKey)
						maxKey = key;
					// printf("%*cKey: %d Value: %d\n", depth*3+2, ' ', key, val);	
					count++;	
				}
			}	

			/* Print summary if do not print all records */		
			printSpaces(depth*3);
			printf("Id: %d Loc: %d Prev: %d Cnt: %d (%d, %d)\n", VMTREE_GET_ID(buffer), pageNum, VMTREE_GET_PREV(buffer), count, minKey, maxKey);	

			/* Print all individual records (optional) */
			for (c=0; c < state->bitmapSize*8 && c < state->maxRecordsPerPage; c++)
			{	/* Must be non-free location (0) and still valid (1) */
				if (bitarrGet(bm1, c) == 0 && bitarrGet(bm2, c) == 1)
				{	/* Found valid record. */	
					/*				
					memcpy(&key, (int32_t*) (buffer + state->headerSize + state->keySize * c), sizeof(int32_t));
					memcpy(&val, (int32_t*) (buffer + state->headerSize + state->maxRecordsPerPage*state->keySize + c * state->dataSize), sizeof(int32_t));
					printSpaces(depth*3+2);
					printf("Key: %lu Value: %lu\n",key, val);	
					*/										
				}
			}			
		}
	}
}

/**
@brief     	Print a node read from storage.
@param     	state
                VMTree algorithm state structure
@param     	pageNum
                Physical page id (number)	
@param     	depth
                Used for nesting print out
*/
void vmtreePrintNode(vmtreeState *state, int pageNum, int depth)
{
	int32_t val;

	pageNum = vmtreeGetMapping(state, pageNum);

	void* buf = readPage(state->buffer, pageNum);
	if (buf == NULL)
	{
		printf("ERROR printing tree. PageNum: %d\n", pageNum);
		return;
	}
	int16_t c, count =  VMTREE_GET_COUNT(buf); 	
	
	/* Track number of nodes at this level */
	state->activePath[depth+1]++;

	vmtreePrintNodeBuffer(state, pageNum, depth, buf);
	if (VMTREE_IS_INTERIOR(buf) && state->levels != 1)
	{				
		if (state->parameters == NOR_OVERWRITE)
		{	
			unsigned char* bm1 = buf + state->interiorHeaderSize - state->interiorBitmapSize*2;
			unsigned char* bm2 = buf + state->interiorHeaderSize - state->interiorBitmapSize;
			
			for (c=0; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
			{	/* Must be non-free location (0) and still valid (1) */
				if (bitarrGet(bm1, c) == 0 && bitarrGet(bm2, c) == 1)
				{	/* Found valid record. */					
					memcpy(&val, ((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->interiorHeaderSize + c*sizeof(id_t))), sizeof(id_t));
					vmtreePrintNode(state, val, depth+1);				
					buf = readPage(state->buffer, pageNum);	
				}
			}			
		}
		else
		{
			for (c=0; c < count && c < state->maxInteriorRecordsPerPage; c++)
			{
				memcpy(&val, ((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t))), sizeof(id_t));
				vmtreePrintNode(state, val, depth+1);				
				buf = readPage(state->buffer, pageNum);			
			}	
			/* Print last child node if active */
			memcpy(&val, ((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t))), sizeof(id_t));	
			vmtreePrintNode(state, val, depth+1);	
		}
	}	
}


/**
@brief     	Print current vmTree as written on storage.
@param     	state
                vmTree algorithm state structure
*/
void vmtreePrint(vmtreeState *state)
{	
	printf("\n\nPrint tree:\n");

	/* Use active path to keep track of stats of nodes at each level */
	for (count_t l=1; l <= state->levels; l++)
		state->activePath[l] = 0;
	vmtreePrintNode(state, state->activePath[0], 0);

	/* Print out number of nodes per level */
	count_t total = 0;	
	for (count_t l=1; l <= state->levels; l++)
	{	printf("Nodes level %d: %lu\n", l, state->activePath[l]);
		total += state->activePath[l];
	}
	printf("Total nodes: %d (%lu)\n", total, state->numNodes);
}

/**
@brief     	Updates the pointers to the current node with latest mappings and removes mapping.
@param     	state
                VMTree algorithm state structure
*/
void vmtreePrintMappings(vmtreeState *state)
{
	/* Prints all active mappings */
	vmtreemapping *mappings = (vmtreemapping*) state->mappingBuffer;

	printf("Mappings:\n");
	
	for (int i=0; i < state->maxMappings; i++)
	{		
		if (mappings[i].prevPage != EMPTY_MAPPING)	
			printf("%lu --> %lu\n", mappings[i].prevPage, mappings[i].currPage);		
	}	
	
	printf("Mapping count: %d  Max: %d\n", state->numMappings, state->maxMappings);	
	printf("Node count: %lu\n", state->numNodes);
}

/**
@brief     	Updates the pointers to the current node with latest mappings and removes mapping.
@param     	state
                VMTree algorithm state structure
@param     	buf
                Buffer containing page
@param     	start
                first pointer index to update
@param     	end
                last pointer index to update (inclusive)
@return		Return number of mappings changed.
*/
count_t vmtreeUpdatePointers(vmtreeState *state, void *buf, count_t start, count_t end)
{	
	// vmtreePrintMappings(state);

	/* Update any stale pointers and remove mappings */
	id_t childIdx, newIdx;
	int num = 0;
	void *ptrOffset = buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage; 
	for (count_t i=start; i <= end; i++)
	{		
		memcpy(&childIdx,((id_t*) (ptrOffset + sizeof(id_t) * (i))), sizeof(id_t));
		
		if (childIdx == state->savedMappingPrev)
			newIdx = state->savedMappingCurr;
		else
			newIdx = vmtreeGetMapping(state, childIdx);
		if (newIdx != childIdx)
		{	/* Update pointer and remove mapping */
			memcpy(((id_t*) (ptrOffset + sizeof(id_t) * (i))), &newIdx, sizeof(id_t));
			vmtreeDeleteMapping(state, childIdx);
			num++;
			// printf("Delete mapping for node: %d  Prev: %d Curr: %d\n", VMTREE_GET_ID(buf), childIdx, newIdx);
		}
	}
	// vmtreePrintMappings(state);
	return num;
}

/**
@brief     	Clears mappings from subtree.
@param     	state
                VMTree algorithm state structure
@param     	pageNum
                Physical page id (number)	
*/
void vmtreeClearMappings(vmtreeState *state, int pageNum)
{
	pageNum = vmtreeGetMapping(state, pageNum);
	// printf("Clear: %lu\n", pageNum);
	void* buf = readPage(state->buffer, pageNum);
	if (buf == NULL)
	{
		printf("ERROR processing page. PageNum: %d\n", pageNum);
		return;
	}
	int16_t c, count =  VMTREE_GET_COUNT(buf); 	

	if (VMTREE_IS_INTERIOR(buf) && state->levels != 1)
	{	int32_t val;		
		for (c=0; c < count && c < state->maxInteriorRecordsPerPage; c++)
		{						
			memcpy(&val, ((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t))), sizeof(id_t));
			
			vmtreeClearMappings(state, val);				
			buf = readPage(state->buffer, pageNum);			
		}	
	
		/* Print last child node if active */
		memcpy(&val, ((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t))), sizeof(id_t));
		if (val != 0)	
		{			
			vmtreeClearMappings(state, val);	
		}
		
		/* Update mappings for this node */
		buf = readPage(state->buffer, pageNum);	
		// printf("Clear mappings\n");
		// vmtreePrintNodeBuffer(state, pageNum, 1, buf);

		count_t num = vmtreeUpdatePointers(state, buf, 0, VMTREE_GET_COUNT(buf));	
		if (num > 0)
		{	/* Changed at least one mapping. Write out updated node. */
			id_t prevId = vmtreeUpdatePrev(state, buf, pageNum);
			id_t currId = writePage(state->buffer, buf);
			// printf("Write node: %lu\n", pageNum);
			if (prevId != state->activePath[0])		// Do not add mapping for the root
				vmtreeAddMapping(state, prevId, currId);				
		}
	}	
}

/**
@brief     	Sets previous id in buffer based on current value or mapping value.
@param     	state
                VMTree algorithm state structure
@param     	buf
                Buffer containing page
@param		currId
				Current id of node
@return    	previous id stored/used in page
*/
id_t vmtreeUpdatePrev(vmtreeState *state, void *buf, id_t currId)
{
	id_t prevId = VMTREE_GET_PREV(buf);
	/* If do not have a mapping for prevId, then use current page num. Not correct to use old prevId as no nodes currently have pointers to it anymore. */
	if (prevId >= PREV_ID_CONSTANT || vmtreeGetMapping(state, prevId) != currId) // vmtreeGetMapping(state, prevId) == prevId)
	{
		prevId = currId;
		VMTREE_SET_PREV(buf, currId);
	}	
	return prevId;
}

/**
@brief     	Updates and fixes mapping after node has been written.
			Note: If mappings are full, may have to write more nodes (recursively to the root)
			until are able to have all mappings correct.
@param     	state
                VMTree algorithm state structure
@param     	prevId
                Previous node id
@param     	currId
                Current node id
*/
void vmtreeFixMappings(vmtreeState *state, id_t prevId, id_t currId, int16_t l)
{
	void *buf;
	
	while (vmtreeAddMapping(state, prevId, currId) == -1 && l >= 0)
	{	/* No more space for mappings. Write all nodes to root until have space for a mapping. */			
		// printf("No more space for mappings. Num: %d Max: %d\n", state->numMappings, state->maxMappings);		
		buf = readPageBuffer(state->buffer, state->activePath[l], 0);	
		if (buf == NULL)
			return;

		state->savedMappingPrev = prevId;
		state->savedMappingCurr = currId;

		prevId = vmtreeUpdatePrev(state, buf, state->activePath[l]);
	
		vmtreeUpdatePointers(state, buf, 0, VMTREE_GET_COUNT(buf));	
		state->savedMappingPrev = EMPTY_MAPPING;
		currId = writePage(state->buffer, buf);
		l--;

		if (l == -1)
		{	/* Wrote new root */
			state->activePath[0] = currId;			
			break;	/* Note: Using break here rather than going to top of loop as do not want to add mapping for root. */
		}
	}
}

/**
@brief     	Sorts a leaf block for NOR ovewriting.			
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@return		Return count of valid records in block.
*/
int16_t vmtreeSortBlockNorOverwrite(vmtreeState *state, void* buf)
{
	int16_t count = 0;
	unsigned char* bm1 = buf + state->headerSize - state->bitmapSize*2;
	unsigned char* bm2 = buf + state->headerSize - state->bitmapSize;			
	
	/* Remove all invalid records */
	for (int16_t c=0; c < state->bitmapSize*8 && c < state->maxRecordsPerPage; c++)
	{	
		if (bitarrGet(bm1, c) == 1)
			break;						/* Hit an empty location. Done. */

		/* Must be non-free location (0) and valid (1) */
		if (bitarrGet(bm2, c) == 1)
		{
			if (count < c)
			{	/* Shift record down */
				memcpy(buf + state->headerSize + state->keySize * count, buf + state->headerSize + state->keySize * c, state->keySize);
				memcpy(buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * count, buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * c, state->dataSize);
			}
			count++;
		}	
	}

	/* Sort data (insertion sort) */
	for (int16_t c=1; c < count; c++)
	{		
		memcpy(state->tempKey, (void*) (buf + state->headerSize + state->keySize * c), state->keySize);
		memcpy(state->tempData, (void*) (buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * c), state->dataSize);

		int16_t c2 = c - 1;

		while (c2 >= 0 && state->compareKey(state->tempKey, (void*) (buf + state->headerSize + state->keySize * c2)) < 0)
		{
			memcpy(buf + state->headerSize + state->keySize * (c2+1), buf + state->headerSize + state->keySize * c2, state->keySize);
			memcpy(buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * (c2+1), buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * c2, state->dataSize);
			c2--;
		}

		memcpy((void*) (buf + state->headerSize + state->keySize * (c2+1)), state->tempKey, state->keySize);
		memcpy((void*) (buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * (c2+1)), state->tempData, state->dataSize);						
	}	

	// vmtreePrintNodeBuffer(state, 1, 1, buf);
	return count;	
}

/**
@brief     	Sorts an interior block for NOR ovewriting.			
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@return		Return count of valid records in block.
*/
int16_t vmtreeSortInteriorBlockNorOverwrite(vmtreeState *state, void* buf)
{
	int16_t count = 0;
	unsigned char* bm1 = buf + state->interiorHeaderSize - state->interiorBitmapSize*2;
	unsigned char* bm2 = buf + state->interiorHeaderSize - state->interiorBitmapSize;			
	
	int8_t ptrsize = sizeof(id_t);

	/* Remove all invalid records */
	for (int16_t c=0; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
	{	
		if (bitarrGet(bm1, c) == 1)
			break;						/* Hit an empty location. Done. */

		/* Must be non-free location (0) and valid (1) */
		if (bitarrGet(bm2, c) == 1)
		{
			if (count < c)
			{	/* Shift record down */
				memcpy(buf + state->interiorHeaderSize + state->keySize * count, buf + state->interiorHeaderSize + state->keySize * c, state->keySize);
				memcpy(buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + ptrsize * count, buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + ptrsize * c, ptrsize);
			}
			count++;			
		}	
	}
	vmtreeSetCountBitsInterior(state, buf, count);
	// vmtreePrintNodeBuffer(state, 1, 1, buf);
	id_t tempdata;
	void *savedkey = malloc(state->keySize);

	/* Sort data (insertion sort) */
	for (int16_t c=1; c < count; c++)
	{		
		memcpy(savedkey, (void*) (buf + state->interiorHeaderSize + state->keySize * c), state->keySize);
		memcpy(&tempdata, (void*) (buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + ptrsize * c), ptrsize);

		int16_t c2 = c - 1;

		while (c2 >= 0 && state->compareKey(savedkey, (void*) (buf + state->interiorHeaderSize + state->keySize * c2)) < 0)
		{
			memcpy(buf + state->interiorHeaderSize + state->keySize * (c2+1), buf + state->interiorHeaderSize + state->keySize * c2, state->keySize);
			memcpy(buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + ptrsize * (c2+1), buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + ptrsize * c2, ptrsize);
			c2--;
		}

		memcpy((void*) (buf + state->interiorHeaderSize + state->keySize * (c2+1)), savedkey, state->keySize);
		memcpy((void*) (buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + ptrsize * (c2+1)), &tempdata, ptrsize);	
	}	
	
	free(savedkey);
	return count;	
}


/**
@brief     	Sets the first count bits to be occupied with valid records. Resets all other bits.			
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@param		key
				key to insert
@param 		left
				left pointer to insert
@param 		right
				right pointer to update for key just larger than key being inserted
@return		Return 1 if success, 0 if failure.
*/
int8_t vmtreeInsertInterior(vmtreeState* state, void *buf, void* key, id_t left, id_t right)
{
	unsigned char* bm1 = buf + state->interiorHeaderSize - state->interiorBitmapSize*2;
	unsigned char* bm2 = buf + state->interiorHeaderSize - state->interiorBitmapSize;			
	int16_t c;
	
	/* Determine key just larger than this one. */
	uint16_t firstloc, loc = 0;
	uint8_t success = 0;
	void *maxkey = NULL;

	/* Find insert location and save info as will update entry for right pointer */	
	for (c=0; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
	{
		if (bitarrGet(bm1, c) == 0 && bitarrGet(bm2, c) == 1)
		{
			if (state->compareKey(key, (void*) (buf + state->interiorHeaderSize + state->keySize * c)) < 0)	
			{
				if (maxkey == NULL || state->compareKey((void*) (buf + state->interiorHeaderSize + state->keySize * c), maxkey) < 0)
				{
					maxkey = (void*) (buf + state->interiorHeaderSize + state->keySize * c);
					loc = c;
				}
			}
		}
		else if (bitarrGet(bm1, c) == 1)
			break;		/* Empty space. Can stop. */
	}

	/* Add two entries (key, left) and (currentKey, right) */	
	for ( ; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
	{	/* Must be free location (1) and still valid (1) */
		if (bitarrGet(bm1, c) == 1 && bitarrGet(bm2, c) == 1)
		{	/* Insert (key, left) record */	
			bitarrSet(bm1, c, 0);
			memcpy(buf + state->interiorHeaderSize + state->keySize * c, key, state->keySize);
			memcpy(buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t)*c, &left, sizeof(id_t));
			success = 1;
			firstloc = c;	
			c++;
			// vmtreePrintNodeBuffer(state, right, 0, buf);
			break;						
		}
	}	

	if (!success)
		return 0;

	/* Add (currentKey, right) */
	for ( ; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
	{	/* Must be free location (1) and still valid (1) */
		if (bitarrGet(bm1, c) == 1 && bitarrGet(bm2, c) == 1)
		{	/* Insert (key, left) record */	
			bitarrSet(bm1, c, 0);
			memcpy(buf + state->interiorHeaderSize + state->keySize * c, buf + state->interiorHeaderSize + state->keySize * loc, state->keySize);
			memcpy(buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t)*c, &right, sizeof(id_t));	
			bitarrSet(bm2, loc, 0);	/* Invalidate previous record */
			// vmtreePrintNodeBuffer(state, right, 0, buf);			
			return 1;						
		}
	}

	/* Remove previous insert. Must do both to be successful. Doing that by setting spot to empty rather than invalidating record. */
	bitarrSet(bm1, firstloc, 1);
	return 0;	
}

/**
@brief     	Insert new key with (left, right) pointers in a new block (so not doing overwrite).	Assumes keys are sorted.
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@param		key
				key to insert
@param 		left
				left pointer to insert
@param 		right
				right pointer to update for key just larger than key being inserted
@return		Return 1 if success, 0 if failure.
*/
int8_t vmtreeInsertInteriorNew(vmtreeState* state, void *buf, int16_t count, void* key, id_t left, id_t right)
{
	// unsigned char* bm1 = buf + state->interiorHeaderSize - state->interiorBitmapSize*2;
	// unsigned char* bm2 = buf + state->interiorHeaderSize - state->interiorBitmapSize;			
	int16_t c;

	/* Find location where insert occurs */
	for (c=0; c < count; c++)
	{	/* Note: Not checking bitmaps as all records are valid and in sorted order */
		if (state->compareKey(key, (void*) (buf + state->interiorHeaderSize + state->keySize * c)) < 0)
			break;						
	}	
	// vmtreePrintNodeBuffer(state, right, 0, buf);	
	if (count > 0)
	{
		/* Move all records back */
		memmove(buf + state->interiorHeaderSize + state->keySize * (c+1), buf + state->interiorHeaderSize + state->keySize * c, state->keySize*(count-c));
		memmove(buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t)*(c+1), 
					buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t)*c, sizeof(id_t)*(count-c));

// 		c--;
	}
	// vmtreePrintNodeBuffer(state, right, 0, buf);	
	/* Add record */
	
	memcpy(buf + state->interiorHeaderSize + state->keySize * c, key, state->keySize);
	memcpy(buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t)*c, &left, sizeof(id_t));
	/* Update other pointer */
	memcpy(buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t)*(c+1), &right, sizeof(id_t));
	
	// vmtreePrintNodeBuffer(state, right, 0, buf);	
				
	return 0;	
}

/**
@brief     	Inserts key/data pair in leaf that is in sorted order.			
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@param		count
				total number of records
@param		key
				key to insert
@param 		data
				data to insert
@return		Return count of valid records in block.
*/
void vmtreeInsertLeaf(vmtreeState* state, void *buf, int16_t count, void* key, void* data)
{
	/* Insertion sort */
	int16_t c = count-1;
	while (c >= 0 && state->compareKey(key, (void*) (buf + state->headerSize + state->keySize * c)) < 0)
	{
		memcpy(buf + state->headerSize + state->keySize * (c+1), buf + state->headerSize + state->keySize * c, state->keySize);
		memcpy(buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * (c+1), buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * c, state->dataSize);
		c--;
	}

	memcpy((void*) (buf + state->headerSize + state->keySize * (c+1)), key, state->keySize);
	memcpy((void*) (buf + state->headerSize + state->keySize * state->maxRecordsPerPage + state->dataSize * (c+1)), data, state->dataSize);							
}

/**
@brief     	Inserts key/data pair in interior that is in sorted order.			
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@param		count
				total number of records
@param		key
				key to insert
@param 		data
				data to insert
@return		Return count of valid records in block.
*/
void vmtreeInsertInteriorSorted(vmtreeState* state, void *buf, int16_t count, void* key, void* data)
{
	/* Insertion sort */
	int16_t c = count-1;
	while (c >= 0 && state->compareKey(key, (void*) (buf + state->interiorHeaderSize + state->keySize * c)) < 0)
	{
		memcpy(buf + state->interiorHeaderSize + state->keySize * (c+1), buf + state->headerSize + state->keySize * c, state->keySize);
		memcpy(buf + state->interiorHeaderSize + state->keySize * state->maxRecordsPerPage + sizeof(id_t) * (c+1), buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * c, sizeof(id_t));
		c--;
	}

	memcpy((void*) (buf + state->interiorHeaderSize + state->keySize * (c+1)), key, state->keySize);
	memcpy((void*) (buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (c+1)), data, sizeof(id_t));							
}

/**
@brief     	Sets the first count bits to be occupied with valid records. Resets all other bits.			
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@return		Return count of valid records in block.
*/
void vmtreeSetCountBitInterior(vmtreeState* state, void *buf, int16_t loc)
{
	unsigned char* bm1 = buf + state->interiorHeaderSize - state->interiorBitmapSize*2;
	bitarrSet(bm1, loc, 0);
}


/**
@brief     	Sets the first count bits to be occupied with valid records. Resets all other bits.			
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@return		Return count of valid records in block.
*/
void vmtreeSetCountBitsLeaf(vmtreeState* state, void *buf, int16_t count)
{
	unsigned char* bm1 = buf + state->headerSize - state->bitmapSize*2;
	unsigned char* bm2 = buf + state->headerSize - state->bitmapSize;			
	int16_t c;

	for (c=0; c < count; c++)
	{
		bitarrSet(bm1, c, 0);	/* Location occupied */
		bitarrSet(bm2, c, 1);	/* Record is currently valid */
	}

	for ( ; c < state->bitmapSize*8 && c < state->maxRecordsPerPage; c++)
	{	
		bitarrSet(bm1, c, 1);	/* Location free */
		bitarrSet(bm2, c, 1);	/* Record is currently valid */		
	}
}

/**
@brief     	Sets the first count bits to be occupied with valid records. Resets all other bits.			
@param     	state
                VMTree algorithm state structure
@param     	buf
                buffer containing block to sort
@return		Return count of valid records in block.
*/
void vmtreeSetCountBitsInterior(vmtreeState* state, void *buf, int16_t count)
{
	unsigned char* bm1 = buf + state->interiorHeaderSize - state->interiorBitmapSize*2;
	unsigned char* bm2 = buf + state->interiorHeaderSize - state->interiorBitmapSize;			
	int16_t c;

	for (c=0; c < count; c++)
	{
		bitarrSet(bm1, c, 0);	/* Location occupied */
		bitarrSet(bm2, c, 1);	/* Record is currently valid */
	}

	for ( ; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
	{	
		bitarrSet(bm1, c, 1);	/* Location free */
		bitarrSet(bm2, c, 1);	/* Record is currently valid */		
	}
}

/**
@brief     	Puts a given key, data pair into structure.
			Support for NOR overwriting. Different page structure.
@param     	state
                VMTree algorithm state structure
@param     	key
                Key for record
@param     	data
                Data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t vmtreePutNorOverwrite(vmtreeState *state, void* key, void *data)
{
	/* Check for capacity. */	
	if (!dbbufferEnsureSpace(state->buffer, 8))	
	{
		printf("Storage is at capacity. Must delete keys.\n");
		return -1;
	}			

	/* Find insert leaf */
	/* Starting at root search for key */
	int8_t 	l;
	void 	*buf, *ptr;	
	id_t  	parent, nextId = state->activePath[0];	
	int32_t pageNum, childNum;

	for (l=0; l < state->levels-1; l++)
	{			
		buf = readPage(state->buffer, nextId);		
		if (buf == NULL)
		{
			printf("ERROR reading page: %d\n", nextId);
			return -1;
		}

		/* Find the key within the node. */
		childNum = vmtreeSearchNode(state, buf, key, nextId, 1);
		if (childNum < 0 || childNum > 10000000)
			childNum = vmtreeSearchNode(state, buf, key, nextId, 1);		
		nextId = getChildPageId(state, buf, nextId, l, childNum);			
		if (nextId == -1)
			return -1;					
		state->activePath[l+1] = nextId;
	}

	/* Read the leaf node */	
	buf = readPageBuffer(state->buffer, nextId, 0);	/* Note: May want to use readPageBuffer in buffer 0 to prevent any concurrency issues instead of readPage. */
	int16_t count;

	/* Append key/data record to first open space in node */
	unsigned char* bm = buf + state->headerSize - state->bitmapSize * 2;		
	
	for (int i=0; i < state->bitmapSize*8 && i < state->maxRecordsPerPage; i++)
	{	/* Free location if bit in count bitmap is 1 */
		if (bitarrGet(bm, i) == 1)
		{	/* Found a spot. Insert key/data record. */
			bitarrSet(bm, i, 0);	

			/* Copy record onto page */		
			ptr = buf + state->headerSize;
			memcpy(ptr + i * state->keySize, key, state->keySize);
			memcpy(ptr + state->maxRecordsPerPage*state->keySize + i * state->dataSize, data, state->dataSize);
			
			/* Write page */
			pageNum = overWritePage(state->buffer, buf, nextId);	
			return 0;
		}
	}	

	/* Current leaf page is full. Perform split. */
	count = vmtreeSortBlockNorOverwrite(state, buf);
	int8_t mid = count/2;
	id_t left, right;
	state->numNodes++;

	/* After split, reset previous node index to unused. */
	VMTREE_SET_PREV(buf, PREV_ID_CONSTANT);
	
	// Invalidate page
	dbbufferSetFree(state->buffer, nextId);
	ptr = buf + state->headerSize;
	int8_t compareKeyMid = state->compareKey(key, (void*) (ptr + state->keySize * mid));

	if (compareKeyMid < 0)
	{	/* Insert key in page with smaller values */
		/* Update bitmaps on page */		
		vmtreeSetCountBitsLeaf(state, buf, mid+1);		
		VMTREE_SET_LEAF(buf);

		/* Buffer key/data record at mid point so do not lose it */	
		memcpy(state->tempKey, ptr + state->keySize * mid, state->keySize);
		memcpy(state->tempData, ptr + state->keySize * state->maxRecordsPerPage + state->dataSize * mid, state->dataSize);

		/* Shift records at and after insert point down one record */		
		vmtreeInsertLeaf(state, buf, mid, key, data);
	
		left = writePage(state->buffer, buf);	
		// vmtreePrintNodeBuffer(state, left, 0, buf);

		/* Copy buffered record to start of block */
		memcpy(buf + state->headerSize, state->tempKey, state->keySize);
		memcpy(buf + state->headerSize + state->keySize * state->maxRecordsPerPage, state->tempData, state->dataSize);

		/* Copy records after mid to start of page */	
		memmove(buf + state->headerSize + state->keySize, buf + state->headerSize + state->keySize*(mid+1), state->keySize*(count-mid));		
		memmove(buf + state->headerSize + state->keySize*state->maxRecordsPerPage + state->dataSize, buf + state->headerSize +state->keySize*state->maxRecordsPerPage + state->dataSize*(mid+1), state->dataSize*(count-mid));				
		
		vmtreeSetCountBitsLeaf(state, buf, count-mid);		
		right = writePage(state->buffer, buf);
		// vmtreePrintNodeBuffer(state, right, 0, buf);
	}
	else
	{	/* Insert key in page with larger values */
		/* Update bitmaps on page */	
		// vmtreePrintNodeBuffer(state, left, 0, buf);	
		vmtreeSetCountBitsLeaf(state, buf, mid+1);	
		VMTREE_SET_LEAF(buf);

		left = writePage(state->buffer, buf);	
		// vmtreePrintNodeBuffer(state, left, 0, buf);

		/* Buffer key/data record at mid point so do not lose it */
		if (state->compareKey(key, (void*) (ptr + state->keySize * (mid+1))) < 0)
		{	/* Middle key to promote is this key. */
			memcpy(state->tempKey, key, state->keySize);
		}
		else
		{
			ptr =  buf + state->headerSize + state->keySize * (mid+1);
			memcpy(state->tempKey, ptr, state->keySize);
		}
		
		/* New split page starts off with original page in buffer. Copy records around as required. */
		/* Copy all records to start of buffer */
		memmove(buf + state->headerSize, buf + state->headerSize + state->keySize*(mid+1), state->keySize*(count-mid-1));		
		memmove(buf + state->headerSize+state->keySize*state->maxRecordsPerPage, buf + state->headerSize +state->keySize*state->maxRecordsPerPage + state->dataSize*(mid+1), state->dataSize*(count-mid-1));				

		/* Insert record onto page */
		vmtreeInsertLeaf(state, buf, (count-mid-1), key, data);			

		vmtreeSetCountBitsLeaf(state, buf, count-mid);	
		right = writePage(state->buffer, buf);
		// vmtreePrintNodeBuffer(state, right, 0, buf);
	}		

	/* Recursively add pointer to parent node. */
	for (l=state->levels-2; l >=0; l--)
	{		
		parent = state->activePath[l];
		/* Special case with memory wrap: Parent node may have been moved due to memory operations in between previous writes. Check mapping to verify it is correct. */
		// TODO: Check if mapping really is needed here. 
		// parent = vmtreeGetMapping(state, parent);
		state->nodeSplitId = parent;

		// Invalidate previous page
		dbbufferSetFree(state->buffer, parent);

		// printf("Here: Left: %d  Right: %d Key: %d  Parent: %d", left, right, *((int32_t*) state->tempKey), parent);

		/* Read parent node */
		buf = readPageBuffer(state->buffer, parent, 0);			/* Forcing read to buffer 0 even if buffered in another buffer as will modify this page. */
		if (buf == NULL)
			return -1;				

		/* Insert new key/pointer pair in node and update pointer of key just larger than this one */
		if (vmtreeInsertInterior(state, buf, state->tempKey, left, right))
		{			
			pageNum = overWritePage(state->buffer, buf, parent);
			return 0;		
		}
	
		/* No space. Split interior node and promote key/pointer pair */
		// printf("Splitting interior node.\n");
		state->numNodes++;
		// vmtreePrintNodeBuffer(state, parent, 0, buf);

		/* After split, reset previous node index to unassigned. */
		VMTREE_SET_PREV(buf, PREV_ID_CONSTANT);

		count = vmtreeSortInteriorBlockNorOverwrite(state, buf)-1;	

		/* TODO: Optimization is to check if have at least two open spaces now that have cleaned up node. If so, then write new node. No need to split further. */
		mid = count/2;
		// if (count % 2 == 0)
		// 	mid--;

		// Invalidate page
		dbbufferSetFree(state->buffer, parent);
		ptr = buf + state->interiorHeaderSize;
		int8_t compareKeyMid = state->compareKey(state->tempKey, (void*) (ptr + state->keySize * mid));	
		int8_t compareKeyMid2 = state->compareKey(state->tempKey, (void*) (ptr + state->keySize * (mid+1)));	
		VMTREE_SET_NOR_INTERIOR(buf);

		if (compareKeyMid < 0)
		{	/* Insert key/pointer in page with smaller values */		
			/* Update count on page then write */			
			vmtreeSetCountBitsInterior(state, buf, mid+2);							
			// vmtreeUpdatePointers(state, buf, 0, count);
				
			/* Buffer key/pointer record at mid point so do not lose it */
			/* TODO: Using tempData here as already using tempKey. This would be a problem if data size is < key size. */
			memcpy(state->tempData, buf + state->interiorHeaderSize + state->keySize * (mid), state->keySize);

			/* TODO: Also buffering next key after mid point as lose it during shift down. Figure out a better way and do not hardcode key type. */
			id_t tempKeyAfterMid;
			memcpy(&tempKeyAfterMid, buf + state->interiorHeaderSize + state->keySize * (mid+1), state->keySize);
			id_t tempPtr;
			memcpy(&tempPtr, buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (mid+1), sizeof(id_t));
			
			vmtreeInsertInteriorNew(state, buf, mid+1, state->tempKey, left, right);	
		
			left = writePage(state->buffer, buf);				
			// vmtreePrintNodeBuffer(state, left, 0, buf);

			vmtreeSetCountBitsInterior(state, buf, count-mid); // +1?
					
			/* Copy buffered pointer to start of block */			
			ptr = buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage;
			memcpy(ptr, &tempPtr, sizeof(id_t));
			memcpy(buf + state->interiorHeaderSize, &tempKeyAfterMid, state->keySize);

			/* Copy records after mid to start of page */					
			memcpy(buf + state->interiorHeaderSize + state->keySize, buf + state->interiorHeaderSize + state->keySize * (mid+2), state->keySize*(count-mid-1));									
			memmove(ptr + sizeof(id_t), ptr + sizeof(id_t) * (mid+2), sizeof(id_t)*(count-mid-1));	 // -1 is new				
								
			// vmtreeUpdatePointers(state, buf, 0, count-mid-1);
			right = writePage(state->buffer, buf);			
			// vmtreePrintNodeBuffer(state, right, 0, buf);

			/* Keep temporary key (move from temp data) */
			memcpy(state->tempKey, state->tempData, state->keySize);
		}
		else
		{	/* Insert key/pointer in page with larger values */
			/* Update count on page then write */
			// if (count % 2 == 0)
			// 	mid++;
			vmtreeSetCountBitsInterior(state, buf, mid+1);						
			// vmtreeUpdatePointers(state, buf, 0, count);						

			ptr = buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage;
	//		if (compareKeyMid2 < 0)
	//		{	/* Promote current key that just got promoted. */
	//			memcpy(state->tempData, state->tempKey, state->keySize);				
//			}
//			else
			{
				/* TODO: Using tempData here as already using tempKey. This would be a problem if data size is < key size. */
				memcpy(state->tempData, buf + state->interiorHeaderSize + state->keySize * (mid), state->keySize);
			}
			
			id_t tmpLeft = writePage(state->buffer, buf);	
			// vmtreePrintNodeBuffer(state, tmpLeft, 0, buf);
						
			if (compareKeyMid2 >= 0)
			{
				/* Copy records after mid to start of page */	
				memcpy(buf + state->interiorHeaderSize, buf + state->interiorHeaderSize + state->keySize * (mid+1), state->keySize*(count-mid));			
				memcpy(ptr, ptr + sizeof(id_t) * (mid+1), sizeof(id_t)*(count-mid));						

				vmtreeSetCountBitsInterior(state, buf, count-mid+1);					
				vmtreeInsertInteriorNew(state, buf, count-mid, state->tempKey, left, right);	
				// vmtreePrintNodeBuffer(state, right, 0, buf);								
			}
			else
			{	// mid--;
				vmtreeSetCountBitsInterior(state, buf, count-mid+1);	
				memcpy(buf + state->interiorHeaderSize+state->keySize, buf + state->interiorHeaderSize + state->keySize * (mid+1), state->keySize*(count-mid));			
				memcpy(ptr + sizeof(id_t), ptr + sizeof(id_t) * (mid+1), sizeof(id_t)*(count-mid));		
				// vmtreePrintNodeBuffer(state, right, 0, buf);
				/* Right pointer is second pointer in node and new key is first key*/
				/* Left pointer is first pointer in the first node */
				memcpy(ptr, &left, sizeof(id_t));
				memcpy(ptr + sizeof(id_t), &right, sizeof(id_t));
				memcpy(buf + state->interiorHeaderSize, state->tempKey, state->keySize);
				// vmtreePrintNodeBuffer(state, right, 0, buf);
			}
			
			// vmtreeUpdatePointers(state, buf, 0, count-mid);
			right = writePage(state->buffer, buf);
			// vmtreePrintNodeBuffer(state, right, 0, buf);

			/* Keep temporary key (move from temp data) */
			left = tmpLeft;
			memcpy(state->tempKey, state->tempData, state->keySize);
		}
	}
	
	/* Special case: Add new root node. */	
	/* Create new root node with the two pointers */
	state->levels++;
	buf = initBufferPage(state->buffer, 0);		
	vmtreeSetCountBitInterior(state, buf, 0);
	vmtreeSetCountBitInterior(state, buf, 1);
	VMTREE_SET_ROOT_NOR(buf);		
	VMTREE_SET_PREV(buf, PREV_ID_CONSTANT);		/* TODO: Determine if need to set previous pointer to state->activePath[0] (previous root). */
	state->numNodes++;

	/* Add key and two pointers */
	memcpy(buf + state->interiorHeaderSize, state->tempKey, state->keySize);  /* NOTE: Need to have a MAX_KEY for right pointer. Nothing to do if init whole buffer to 1s at start. */
	ptr = buf + state->interiorHeaderSize + state->keySize * state->maxInteriorRecordsPerPage;
	memcpy(ptr, &left, sizeof(id_t));
	memcpy(ptr + sizeof(id_t), &right, sizeof(id_t));

	state->activePath[0] = writePage(state->buffer, buf);	
	// vmtreePrintNodeBuffer(state, state->activePath[0], 0, buf);
	return 0;
}

/**
@brief     	Puts a given key, data pair into structure.
@param     	state
                VMTree algorithm state structure
@param     	key
                Key for record
@param     	data
                Data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t vmtreePut(vmtreeState *state, void* key, void *data)
{
	/* Check for capacity. */	
	if (!dbbufferEnsureSpace(state->buffer, 8))	
	{
		printf("Storage is at capacity. Must delete keys.\n");
		return -1;
	}			

	if (state->parameters == NOR_OVERWRITE)
		return vmtreePutNorOverwrite(state, key, data);
	
	/* Find insert leaf */
	/* Starting at root search for key */
	int8_t 	l;
	void 	*buf, *ptr;	
	id_t  	prevId, parent, nextId = state->activePath[0];	
	int32_t pageNum, childNum;

	for (l=0; l < state->levels-1; l++)
	{					
		buf = readPage(state->buffer, nextId);		
		if (buf == NULL)
		{
			printf("ERROR reading page: %lu\n", nextId);
			return -1;
		}

		// Find the key within the node. Sorted by key. Use binary search. 
		childNum = vmtreeSearchNode(state, buf, key, nextId, 1);
		if (childNum < 0 || childNum > 10000000)
			childNum = vmtreeSearchNode(state, buf, key, nextId, 1);
		nextId = getChildPageId(state, buf, nextId, l, childNum);		
		if (nextId == -1)
			return -1;					
		state->activePath[l+1] = nextId;
	}

	/* Read the leaf node */
	buf = readPageBuffer(state->buffer, nextId, 0);	/* Note: May want to use readPageBuffer in buffer 0 to prevent any concurrency issues instead of readPage. */
	int16_t count =  VMTREE_GET_COUNT(buf); 
	state->nodeSplitId = nextId;

	childNum = -1;
	if (count > 0)
		childNum = vmtreeSearchNode(state, buf, key, nextId, 1);
		
	ptr = buf + state->headerSize + state->recordSize * (childNum+1);
	if (count < state->maxRecordsPerPage)
	{	/* Space for record on leaf node. */
		/* Insert record onto page in sorted order */		
		/* Shift records down */			
		if (count-childNum-1 > 0)
		{
			memmove(ptr + state->recordSize, ptr, state->recordSize*(count-childNum-1));					
		}
		
		/* Copy record onto page */		
		memcpy(ptr, key, state->keySize);
		memcpy(ptr + state->keySize, data, state->dataSize);

		/* Update count */
		VMTREE_INC_COUNT(buf);	

		/* Write updated page */		
		if (state->parameters != OVERWRITE)
		{
			// Invalidate page
			dbbufferSetFree(state->buffer, nextId);

			/* Write updated page */		
			if (state->levels == 1)
			{	/* Wrote to root */
				state->activePath[0] = writePage(state->buffer, buf);						
			}
			else
			{	
				/* Set previous id in page if does not have one currently */
				prevId = vmtreeUpdatePrev(state, buf, nextId);			

				pageNum = writePage(state->buffer, buf);

				/* Add/update mapping */	
				l=state->levels-2;	
				vmtreeFixMappings(state, prevId, pageNum, l);									
			}
		}
		else
		{	/* Overwrite */
			/* Write updated page */	
			pageNum = overWritePage(state->buffer, buf, nextId);				
		}
		return 0;
	}
	
	/* Current leaf page is full. Perform split. */
	int8_t mid = count/2;
	id_t left, right;
	state->numNodes++;

	/* After split, reset previous node index to unused. */
	VMTREE_SET_PREV(buf, PREV_ID_CONSTANT);
	
	if (childNum < mid)
	{	/* Insert key in page with smaller values */
		/* Update count on page then write */
		VMTREE_SET_COUNT(buf, mid+1);	
		
		/* Buffer key/data record at mid point so do not lose it */
		ptr = buf + state->headerSize + state->recordSize * mid;
		memcpy(state->tempKey, ptr, state->keySize);
		memcpy(state->tempData, ptr + state->keySize, state->dataSize);

		/* Shift records at and after insert point down one record */
		ptr =  buf + state->headerSize + state->recordSize * (childNum+1);
		if ((mid-childNum-1) > 0)
			memmove(ptr + state->recordSize, ptr, state->recordSize*(mid-childNum-1));		

		/* Copy record onto page */
		memcpy(ptr, key, state->keySize);
		memcpy(ptr + state->keySize, data, state->dataSize);

		left = writePage(state->buffer, buf);	
		// vmtreePrintNodeBuffer(state, left, 0, buf);

		/* Copy buffered record to start of block */
		memcpy(buf + state->headerSize, state->tempKey, state->keySize);
		memcpy(buf + state->headerSize + state->keySize, state->tempData, state->dataSize);

		/* Copy records after mid to start of page */	
		memcpy(buf + state->headerSize + state->recordSize, buf + state->headerSize + state->recordSize * (mid+1), state->recordSize*(count-mid));		
		
		VMTREE_SET_COUNT(buf, count-mid);
		right = writePage(state->buffer, buf);
		// vmtreePrintNodeBuffer(state, right, 0, buf);
	}
	else
	{	/* Insert key in page with larger values */
		/* Update count on page then write */
		VMTREE_SET_COUNT(buf, mid+1);

		left = writePage(state->buffer, buf);	
		// vmtreePrintNodeBuffer(state, left, 0, buf);

		/* Buffer key/data record at mid point so do not lose it */
		if (childNum == mid)
		{	/* Middle key to promote is this key. */
			memcpy(state->tempKey, key, state->keySize);
		}
		else
		{
			ptr =  buf + state->headerSize + state->recordSize * (mid+1);
			memcpy(state->tempKey, ptr, state->keySize);
		}
		
		/* New split page starts off with original page in buffer. Copy records around as required. */
		/* Copy records before insert point into front of block from current location in block */
		if ((childNum-mid) > 0)
			memcpy(buf + state->headerSize, ptr, state->recordSize*(childNum-mid));		

		/* Copy record onto page */
		ptr = buf + state->headerSize + state->recordSize * (childNum-mid);
		memcpy(ptr, key, state->keySize);
		memcpy(ptr + state->keySize, data, state->dataSize);

		/* Copy records after insert point after value just inserted */
		memcpy(buf + state->headerSize + state->recordSize * (childNum-mid+1), buf + state->headerSize + state->recordSize * (childNum+1), state->recordSize*(count-childNum-1));	

		VMTREE_SET_COUNT(buf, count-mid);
		right = writePage(state->buffer, buf);
		// vmtreePrintNodeBuffer(state, right, 0, buf);
	}		

	/* Recursively add pointer to parent node. */
	for (l=state->levels-2; l >=0; l--)
	{		
		parent = state->activePath[l];
		/* Special case with memory wrap: Parent node may have been moved due to memory operations in between previous writes. Check mapping to verify it is correct. */
		// TODO: Check if mapping really is needed here. 
		// parent = vmtreeGetMapping(state, parent);
		state->nodeSplitId = parent;

		// Invalidate previous page
		dbbufferSetFree(state->buffer, parent);

		// printf("Here: Left: %d  Right: %d Key: %d  Parent: %d", left, right, *((int32_t*) state->tempKey), parent);

		/* Read parent node */
		buf = readPageBuffer(state->buffer, parent, 0);			/* Forcing read to buffer 0 even if buffered in another buffer as will modify this page. */
		if (buf == NULL)
			return -1;				

		int16_t count =  VMTREE_GET_COUNT(buf); 
		if (count < state->maxInteriorRecordsPerPage)
		{	/* Space for key/pointer in page */
			childNum = vmtreeSearchNode(state, buf, state->tempKey, parent, 1);

			vmtreeUpdatePointers(state, buf, 0, count);			
	
			/* Note: memcpy with overlapping ranges. May be an issue on some platforms */
			ptr = buf + state->headerSize + state->keySize * (childNum);
			/* Shift down all keys */
			memmove(ptr + state->keySize, ptr, state->keySize * (count-childNum));		

			/* Insert key in page */			
			memcpy(ptr, state->tempKey, state->keySize);

			/* Shift down all pointers */
			ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * childNum;
			memmove(ptr + sizeof(id_t), ptr, sizeof(id_t)*(count-childNum+1));

			/* Insert pointer in page */			
			memcpy(ptr, &left, sizeof(id_t));
			memcpy(ptr + sizeof(id_t), &right, sizeof(id_t));

			VMTREE_INC_COUNT(buf);			

			/* Write page */
			/* Set previous id in page if does not have one currently */
			prevId = vmtreeUpdatePrev(state, buf, parent);				
			
			if (state->parameters != OVERWRITE)
			{
				pageNum = writePage(state->buffer, buf);							

				if (l == 0)
				{	/* Update root */
					state->activePath[0] = pageNum;
				}
				else
				{	/* Add a mapping for new page location */								
					l--;
					vmtreeFixMappings(state, prevId, pageNum, l);											
				}
			}
			else
			{	/* Overwrite */
				pageNum = overWritePage(state->buffer, buf, parent);
			}
			return 0;
		}

		/* No space. Split interior node and promote key/pointer pair */
		// printf("Splitting interior node.\n");
		state->numNodes++;

		/* After split, reset previous node index to unassigned. */
		VMTREE_SET_PREV(buf, PREV_ID_CONSTANT);

		childNum = -1;
		if (count > 0)
			childNum = vmtreeSearchNode(state, buf, state->tempKey, parent, 1);
 		mid = count/2;

		if (childNum < mid)
		{	/* Insert key/pointer in page with smaller values */
			/* Update count on page then write */
			if (count % 2 == 0)
				mid--;	/* Note: If count is odd, then first node will have extra key/pointer */
			VMTREE_SET_COUNT(buf, mid + 1);	 			
			VMTREE_SET_INTERIOR(buf);  
			vmtreeUpdatePointers(state, buf, 0, count);

			/* Buffer key/pointer record at mid point so do not lose it */
			/* TODO: Using tempData here as already using tempKey. This would be a problem if data size is < key size. */
			memcpy(state->tempData, buf + state->headerSize + state->keySize * (mid), state->keySize);
			id_t tempPtr;
			memcpy(&tempPtr, buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (mid+1), sizeof(id_t));

			/* Copy keys and pointers after insert point down one from current location in block */
			ptr = buf + state->headerSize + state->keySize * childNum;
			if ((mid-childNum) > 0)
			{
				/* Shift down all keys */
				memmove(ptr + state->keySize, ptr, state->keySize*(mid-childNum));

				/* Shift down all pointers */
				ptr = buf + state->headerSize  + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (childNum+1);
				memmove(ptr + sizeof(id_t), ptr, sizeof(id_t)*(mid-childNum));		
			}				

			/* Copy record onto page */
			ptr = buf + state->headerSize + state->keySize * childNum;
			memcpy(ptr, state->tempKey, state->keySize);
			ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (childNum);
			memcpy(ptr, &left, sizeof(id_t));
			memcpy(ptr + sizeof(id_t), &right, sizeof(id_t));

			left = writePage(state->buffer, buf);				
					
			/* Copy buffered pointer to start of block */			
			ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage;
			memcpy(ptr, &tempPtr, sizeof(id_t));

			/* Copy records after mid to start of page */	
			memcpy(buf + state->headerSize, buf + state->headerSize + state->keySize * (mid+1), state->keySize*(count-mid-1));			
			memcpy(ptr + sizeof(id_t), ptr + sizeof(id_t) * (mid+2), sizeof(id_t)*(count-mid-1));		
			
			VMTREE_SET_COUNT(buf, count-mid-1);
			VMTREE_SET_INTERIOR(buf);
			// vmtreeUpdatePointers(state, buf, 0, count-mid-1);
			right = writePage(state->buffer, buf);			

			/* Keep temporary key (move from temp data) */
			memcpy(state->tempKey, state->tempData, state->keySize);
		}
		else
		{	/* Insert key/pointer in page with larger values */
			/* Update count on page then write */
			VMTREE_SET_COUNT(buf, mid);
			VMTREE_SET_INTERIOR(buf);
			vmtreeUpdatePointers(state, buf, 0, count);						

			ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage;
			if (childNum == mid)
			{	/* Promote current key that just got promoted. */
				memcpy(state->tempData, state->tempKey, state->keySize);
				/* Left pointer is last pointer in the first node */
				memcpy(ptr + sizeof(id_t) * mid, &left, sizeof(id_t));
			}
			else
			{
				/* TODO: Using tempData here as already using tempKey. This would be a problem if data size is < key size. */
				memcpy(state->tempData, buf + state->headerSize + state->keySize * (mid), state->keySize);
			}
			id_t tempPtr;
			memcpy(&tempPtr, buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (mid+1), sizeof(id_t));
			
			id_t tmpLeft = writePage(state->buffer, buf);	
			// vmtreePrintNodeBuffer(state, tmpLeft, 0, buf);
						
			/* New split page starts off with original page in buffer. Copy records around as required. */
			/* Copy records before insert point into front of block from current location in block */			
			if ((childNum-mid-1) > 0)
			{
				memcpy(buf + state->headerSize, buf + state->headerSize + state->keySize * (mid+1), state->keySize*(childNum-mid-1));	
				memcpy(ptr, ptr + sizeof(id_t) * (mid+1), sizeof(id_t)*(childNum-mid-1));		
			}	  
	 
			
			if (childNum > mid)
			{
				/* Copy record onto page */
				memcpy(buf + state->headerSize + state->keySize * (childNum-mid-1), state->tempKey, state->keySize);
				/* Right pointer */
				memcpy(ptr + sizeof(id_t) * (childNum-mid-1), &left, sizeof(id_t));
			}
			memcpy(ptr + sizeof(id_t) * (childNum-mid), &right, sizeof(id_t));

			/* Copy records after insert point after value just inserted */
			if (count-childNum > 0)
			{
				memcpy(buf + state->headerSize + state->keySize * (childNum-mid), buf + state->headerSize + state->keySize * (childNum), state->keySize*(count-childNum));	
				memcpy(ptr + sizeof(id_t) * (childNum-mid+1), ptr + sizeof(id_t) * (childNum+1), sizeof(id_t)*(count-childNum));	
			}
	
			VMTREE_SET_COUNT(buf, count-mid);
			VMTREE_SET_INTERIOR(buf);
			// vmtreeUpdatePointers(state, buf, 0, count-mid);

			right = writePage(state->buffer, buf);
			// vmtreePrintNodeBuffer(state, right, 0, buf);

			/* Keep temporary key (move from temp data) */
			left = tmpLeft;
			memcpy(state->tempKey, state->tempData, state->keySize);
		}
	}
	
	/* Special case: Add new root node. */	
	/* Create new root node with the two pointers */
	buf = initBufferPage(state->buffer, 0);	
	VMTREE_SET_COUNT(buf, 1);
	VMTREE_SET_ROOT(buf);		
	VMTREE_SET_PREV(buf, PREV_ID_CONSTANT);		/* TODO: Determine if need to set previous pointer to state->activePath[0] (previous root). */
	state->numNodes++;
	
	/* Add key and two pointers */
	memcpy(buf + state->headerSize, state->tempKey, state->keySize);
	ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage;
	memcpy(ptr, &left, sizeof(id_t));
	memcpy(ptr + sizeof(id_t), &right, sizeof(id_t));

	state->activePath[0] = writePage(state->buffer, buf);
	state->levels++;
	// vmtreePrintNodeBuffer(state, state->activePath[0], 0, buf);
	return 0;
}

/**
@brief     	Given a key, searches the node for the key.
			Different search algorithm for overwrite nodes that have different structure and non-sorted keys.
			If interior node, returns child record number containing next page id to follow.
			If leaf node, returns if of first record with that key or (<= key).
			Returns -1 if key is not found.			
@param     	state
                VMTree algorithm state structure
@param     	buffer
                Pointer to in-memory buffer holding node
@param     	key
                Key for record
@param		pageId
				Page if for page being searched
@param		range
				1 if range query so return pointer to first record <= key, 0 if exact query so much return first exact match record
*/
int32_t vmtreeSearchNodeOverwrite(vmtreeState *state, void *buffer, void* key, id_t pageId, int8_t range)
{
	int8_t compare, interior = VMTREE_IS_INTERIOR(buffer) && state->levels != 1;

	if (interior)
	{
		unsigned char* bm1 = buffer + state->interiorHeaderSize - state->interiorBitmapSize*2;
		unsigned char* bm2 = buffer + state->interiorHeaderSize - state->interiorBitmapSize;
		// bitarrPrint(bm1, state->maxRecordsPerPage);
		// bitarrPrint(bm2, state->maxRecordsPerPage);
		int16_t loc = 0;
		void* minkey = NULL;
		for (int16_t c=0; c < state->interiorBitmapSize*8 && c < state->maxInteriorRecordsPerPage; c++)
		{	/* Must be non-free location (0) and still valid (1) */
			if (bitarrGet(bm1, c) == 0 && bitarrGet(bm2, c) == 1)
			{	/* Found valid record. */					
				void* mkey = (void*) (buffer+state->keySize * c + state->interiorHeaderSize);	

				if (state->compareKey(key,mkey) < 0)
				{	/* Search key is less than this key*/
					if (minkey == NULL || state->compareKey(mkey, minkey) < 0)
					{	/* New key is smaller than previous min */
						minkey = mkey;
						loc = c;
					}

				}								
			}
		}		
		return loc;
	}
	else
	{
		unsigned char* bm1 = buffer + state->headerSize - state->bitmapSize*2;
		unsigned char* bm2 = buffer + state->headerSize - state->bitmapSize;
		// bitarrPrint(bm1, state->maxRecordsPerPage);
		// bitarrPrint(bm2, state->maxRecordsPerPage);
		for (int16_t c=0; c < state->bitmapSize*8 && c < state->maxRecordsPerPage; c++)
		{	/* Must be non-free location (0) and still valid (1) */
			if (bitarrGet(bm1, c) == 0 && bitarrGet(bm2, c) == 1)
			{	/* Found valid record. */					
				void* mkey = (void*) (buffer+state->keySize * c + state->headerSize);

				compare = state->compareKey(key,mkey);
				if (compare == 0)
					return c;				
			}
		}		
	}
	return 0;
}

/**
@brief     	Given a key, searches the node for the key.
			If interior node, returns child record number containing next page id to follow.
			If leaf node, returns if of first record with that key or (<= key).
			Returns -1 if key is not found.			
@param     	state
                VMTree algorithm state structure
@param     	buffer
                Pointer to in-memory buffer holding node
@param     	key
                Key for record
@param		pageId
				Page if for page being searched
@param		range
				1 if range query so return pointer to first record <= key, 0 if exact query so much return first exact match record
*/
int32_t vmtreeSearchNode(vmtreeState *state, void *buffer, void* key, id_t pageId, int8_t range)
{
	if (state->parameters == NOR_OVERWRITE)
		return vmtreeSearchNodeOverwrite(state, buffer, key, pageId, range);

	int16_t first, last, middle, count;
	int8_t compare, interior;
	void *mkey;
	
	count = VMTREE_GET_COUNT(buffer);  
	interior = VMTREE_IS_INTERIOR(buffer) && state->levels != 1;
	
	if (interior)
	{
		if (count == 0)	/* Only one child pointer */
			return 0;
		if (count == 1)	/* One key and two children pointers */
		{
			mkey = buffer+state->headerSize;   /* Key at index 0 */
			compare = state->compareKey(key, mkey);
			if (compare < 0)
				return 0;
			return 1;		
		}
		
		first = 0;	
  		last =  count;
		if (last > state->maxInteriorRecordsPerPage)
			last = state->maxInteriorRecordsPerPage;
  		middle = (first+last)/2;
		while (first < last) 
		{			
			mkey = buffer+state->headerSize+state->keySize*middle;
			compare = state->compareKey(key,mkey);
			if (compare > 0)
				first = middle + 1;
			else if (compare == 0) 
			{	last = middle+1; /* Return the child pointer just after */
				break;
			}				
			else
				last = middle;  /* Note: Not -1 as always want last pointer to be <= key so that will use it if necessary */

			middle = (first + last)/2;
		}
		return last;		
	}
	else
	{
		first = 0;	
  		last =  count - 1;
  		middle = (first+last)/2;	

		while (first <= last) 
		{			
			mkey = buffer+state->headerSize+state->recordSize*middle;		
			compare = state->compareKey(mkey, key);
			
			if (compare < 0)
				first = middle + 1;
			else if (compare == 0) 
				return middle;							
			else
				last = middle - 1;

			middle = (first + last)/2;
		}
		if (range)			
		{	// return middle;
			if (last == -1)
				return -1;
			return middle;
		}
		return -1;
	}
}

/**
@brief     	Given a child link, returns the proper physical page id.
			This method handles the mapping of the active path where the pointer in the
			node is not actually pointing to the most up to date block.			
@param     	state
                VMTree algorithm state structure
@param		buf
				Buffer containing node
@param     	pageId
                Page id for node
@param     	level
                Level of node in tree
@param		childNum
				Child pointer index
@return		Return pageId if success or -1 if not valid.
*/
id_t getChildPageId(vmtreeState *state, void *buf, id_t pageId, int8_t level, id_t childNum)
{		
	/* Retrieve page number for child */
	id_t nextId;
	memcpy(&nextId, (id_t*) (buf + state->interiorHeaderSize + state->keySize*state->maxInteriorRecordsPerPage + sizeof(id_t)*childNum), sizeof(id_t));
	if (state->parameters != NOR_OVERWRITE)
	{
		// if (nextId == 0 && childNum==(VMTREE_GET_COUNT(buf)))	/* Last child which is empty */
		if (childNum > (VMTREE_GET_COUNT(buf)))	/* Last child which is empty */
			return -1;
	}

	/* Perform mapping */	
	nextId = vmtreeGetMapping(state, nextId);
	return nextId;
}

/**
@brief     	Given a key, returns data associated with key.
			Note: Space for data must be already allocated.
			Data is copied from database into data buffer.
@param     	state
                VMTree algorithm state structure
@param     	key
                Key for record
@param     	data
                Pre-allocated memory to copy data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t vmtreeGet(vmtreeState *state, void* key, void *data)
{
	/* Starting at root search for key */
	int8_t l;
	void *buf;
	id_t childNum, nextId = state->activePath[0];

	for (l=0; l < state->levels-1; l++)
	{		
		buf = readPage(state->buffer, nextId);		

		/* Find the key within the node. Sorted by key. Use binary search. */
		childNum = vmtreeSearchNode(state, buf, key, nextId, 0);
		nextId = getChildPageId(state, buf, nextId, l, childNum);
		if (nextId == -1)
			return -1;		
	}

	/* Search the leaf node and return search result */
	buf = readPage(state->buffer, nextId);
	if (buf == NULL)
		return -1;
	nextId = vmtreeSearchNode(state, buf, key, nextId, 0);

	if (nextId != -1)
	{	/* Key found */
		if (state->parameters != NOR_OVERWRITE)
			memcpy(data, (void*) (buf+state->headerSize+state->recordSize*nextId+state->keySize), state->dataSize);
		else
			memcpy(data, (void*) (buf+state->headerSize+state->dataSize*nextId+state->keySize*state->maxRecordsPerPage), state->dataSize);
		return 0;
	}
	return -1;
}

/**
@brief     	Flushes output buffer.
@param     	state
                VMTree algorithm state structure
*/
int8_t vmtreeFlush(vmtreeState *state)
{
	/* TODO: Needs to be implemented. There is currently no output write buffer. Each write is done immediately. */
	// int32_t pageNum = writePage(state->buffer, state->writeBuffer);	

	/* Add pointer to page to B-tree structure */		
	/* So do not have to allocate memory. Use the next key value in the buffer temporarily to store a MAX_KEY of all 1 bits */	
	/* Need to copy key from current write buffer as will reuse buffer */
	/*
	memcpy(state->tempKey, (void*) (state->buffer+state->headerSize), state->keySize); 	
	void *maxkey = state->buffer + state->recordSize * vmTREE_GET_COUNT(state->buffer) + state->headerSize;
	memset(maxkey, 1, state->keySize);
	 vmtreeUpdateIndex(state, state->tempKey, maxkey, pageNum);
	*/
	// TODO: Look at what the key should be when flush. Needs to be one bigger than data set 

	// void *maxkey = state->writeBuffer + state->recordSize * (VMTREE_GET_COUNT(state->writeBuffer)-1) + state->headerSize;
	// int32_t mkey = *((int32_t*) maxkey)+1;
	// maxkey = state->writeBuffer + state->headerSize;
	// int32_t minKey = *((int32_t*) maxkey);
//	if (vmtreeUpdateIndex(state, &minKey, &mkey, pageNum) != 0)
//		return -1;
		
	// fflush(state->buffer->file);

	/* Reinitialize buffer */
	// initBufferPage(state->buffer, 0);
	return 0;
}


/**
@brief     	Initialize iterator on VMTree structure.
@param     	state
                VMTree algorithm state structure
@param     	it
                VMTree iterator state structure
*/
void vmtreeInitIterator(vmtreeState *state, vmtreeIterator *it)
{	
	/* Find start location */
	/* Starting at root search for key */
	int8_t l;
	void *buf;	
	id_t childNum, nextId = state->activePath[0];
	it->currentBuffer = NULL;

	for (l=0; l < state->levels-1; l++)
	{		
		it->activeIteratorPath[l] = nextId;		
		buf = readPage(state->buffer, nextId);		

		/* Find the key within the node. Sorted by key. Use binary search. */
		childNum = vmtreeSearchNode(state, buf, it->minKey, nextId, 1);
		nextId = getChildPageId(state, buf, nextId, l, childNum);
		if (nextId == -1)
			return;	
		
		it->lastIterRec[l] = childNum;
	}

	/* Search the leaf node and return search result */
	it->activeIteratorPath[l] = nextId;	
	buf = readPage(state->buffer, nextId);
	it->currentBuffer = buf;
	childNum = vmtreeSearchNode(state, buf, it->minKey, nextId, 1);		
	it->lastIterRec[l] = childNum;
}


/**
@brief     	Requests next key, data pair from iterator.
@param     	state
                vmTree algorithm state structure
@param     	it
                vmTree iterator state structure
@param     	key
                Key for record (pointer returned)
@param     	data
                Data for record (pointer returned)
*/
int8_t vmtreeNext(vmtreeState *state, vmtreeIterator *it, void **key, void **data)
{	
	void *buf = it->currentBuffer;
	int8_t l=state->levels-1;
	id_t nextPage;

	/* No current page to search */
	if (buf == NULL)
		return 0;

	/* Iterate until find a record that matches search criteria */
	while (1)
	{	
		if (it->lastIterRec[l] >= VMTREE_GET_COUNT(buf))
		{	/* Read next page */						
			it->lastIterRec[l] = 0;

			while (1)
			{
				/* Advance to next page. Requires examining active path. */
				for (l=state->levels-2; l >= 0; l--)
				{	
					buf = readPage(state->buffer, it->activeIteratorPath[l]);
					if (buf == NULL)
						return 0;						

					int8_t count = VMTREE_GET_COUNT(buf);
					if (l == state->levels-1)
						count--;
					if (it->lastIterRec[l] < count)
					{
						it->lastIterRec[l]++;
						break;
					}
					it->lastIterRec[l] = 0;
				}
				if (l == -1)
					return 0;		/* Exhausted entire tree */

				for ( ; l < state->levels-1; l++)
				{						
					nextPage = it->activeIteratorPath[l];
					nextPage = getChildPageId(state, buf, nextPage, l, it->lastIterRec[l]);
					if (nextPage == -1)
						return 0;	
					
					it->activeIteratorPath[l+1] = nextPage;
					buf = readPage(state->buffer, nextPage);
					if (buf == NULL)
						return 0;	
				}
				it->currentBuffer = buf;

				/* TODO: Check timestamps, min/max, and bitmap to see if query range overlaps with range of records	stored in block */
				/* If not read next block */
				break;										
			}
		}
		
		/* Get record */	
		// vmtreePrintNodeBuffer(state, 0, 0, buf);
		*key = buf+state->headerSize+it->lastIterRec[l]*state->recordSize;
		*data = *key+state->keySize;
		it->lastIterRec[l]++;
		
		/* Check that record meets filter constraints */
		if (it->minKey != NULL && state->compareKey(*key, it->minKey) < 0)
			continue;
		if (it->maxKey != NULL && state->compareKey(*key, it->maxKey) > 0)
			return 0;	/* Passed maximum range */
		return 1;
	}
}


/**
@brief     	Given a physical page number, returns 0 if valid, -1 if no longer used.
@param     	state
                VMTree algorithm state structure
@param		pageNum
				Physical page number
@param		parentId
				Physical page number of parent
@param		parentBuffer
				Returns pointer to buffer containing parent node if found, NULL otherwise.
@return		Returns 0 if page is valid, -1 if no longer used.
*/
int8_t vmtreeIsValid(void *statePtr, id_t pageNum, id_t *parentId, void **parentBuffer)
{	
	vmtreeState *state = statePtr;

	int8_t response = dbbufferIsFree(state->buffer, pageNum);
	if (response == 0)
		return 0;
	else
	{
		/* Check if there is mapping from page num */
		id_t mapId = vmtreeGetMapping(state, pageNum);
		if (mapId != pageNum)
			return 1;			/* Mapping exists */
		return -1;
	}
	
	if (response == 1)
		return -1;
	return 0;

	/* INFO: Code below here would search for page in tree rather than using bit vector tracking free space */	
	*parentId = 0;	
	*parentBuffer = NULL;

	/* Search to see if page is still in tree */
	void *buf = readPage(state->buffer, pageNum);
	if (buf == NULL)
		return -1;

	/* TODO: Cannot be hardcoded key. Cannot use state->tempKey. */
	int32_t key;
	/* Retrieve minimum key to search for */
	/* Copying tree off page as will need to read other pages and will most likely lose page in buffer */
	memcpy(&key, vmtreeGetMinKey(state, buf), state->keySize);

	/* This code is almost identical to vmtreeGet as searching but duplicated it for now as can stop early if find node. */
	/* May be a candidate for code refactorization to avoid this duplication. */
	/* Starting at root search for key */
	int8_t l;	
	id_t childNum, nextId = state->activePath[0];	

	if (nextId == pageNum)
		return -1;		

	id_t mapId = vmtreeGetMapping(state, pageNum);

	for (l=0; l < state->levels-1; l++)
	{		
		buf = readPage(state->buffer, nextId);		

		/* Find the key within the node. */
		childNum = vmtreeSearchNode(state, buf, &key, nextId, 0);
		*parentId = nextId;
		*parentBuffer = buf;
		nextId = getChildPageId(state, buf, nextId, l, childNum);
		if (nextId == pageNum)						
			return 0;		/* Page is valid page currently in tree */
		if (mapId != pageNum && mapId == nextId)
			return 1;		/* Page is no longer valid, but a mapping exists from the original page number to a new page */
		if (nextId == -1)
			break;
	}
	
	return -1;
}


/**
@brief     	Informs the VMTree that the buffer moved a page from prev to curr location.
			It must update any mappings if required.
@param     	state
                VMTree algorithm state structure
@param		prev
				Previous physical page number
@param		curr
				Previous physical page number
@param		buf
				Buffer containing the page
*/
void vmtreeMovePage(void *state, id_t prev, id_t curr, void *buf)
{
	// vmtreePrintNodeBuffer(state, prev, 0, buf);
	/* Update the mapping. */
	if (VMTREE_IS_INTERIOR(buf))
	{
		// printf("Updating mappings.\n");		
		vmtreeUpdatePointers(state, buf, 0, VMTREE_GET_COUNT(buf));
		vmtreePrintNodeBuffer(state, prev, 0, buf);
	}

	if (((vmtreeState*) state)->activePath[0] == prev)
	{	/* Modified root location */
		((vmtreeState*) state)->activePath[0] = curr;
	}
	else
	{
		prev = vmtreeUpdatePrev(state, buf, prev);
		/* TODO: Handle case when not enough mapping space. */
		if (vmtreeAddMapping(state, prev, curr) == -1)
		{
			printf("ERROR: Ran out of mapping space.\n");
			vmtreePrintMappings(state);
		}
	}
	// vmtreePrintMappings(state);
}