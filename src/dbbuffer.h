/******************************************************************************/
/**
@file		dbbuffer.h
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
#ifndef DBBUFFER_H
#define DBBUFFER_H

#include <stdint.h>
#include <stdio.h>

#include "bitarr.h"
#include "storage.h"

/* Define type for page ids (physical and logical). */
typedef uint32_t id_t;

/* Define type for page record count. */
typedef uint16_t count_t;

typedef struct {
	id_t*  	status;					/* Contents of buffer (physical page id)  */    
	void*  	buffer;					/* Allocated memory for buffer */
	count_t	pageSize;				/* Size of buffer page */
	count_t	numPages;				/* Number of buffer pages */    
	storageState* storage;			/* Storage information for reading/writing pages */
	count_t eraseSizeInPages;		/* Erase size in pages */
	id_t 	endDataPage;			/* End data page number */		
	id_t 	erasedEndPage;			/* Physical page number of last page erased */	
	id_t	erasedStartPage;		/* Physical page number of first page in next erased block */	
	int8_t 	wrappedMemory;			/* 1 if have wrapped around in memory, 0 otherwise */
	id_t 	nextPageId;				/* Next logical page id. Page id is an incrementing value and may not always be same as physical page id. */
	id_t 	nextPageWriteId;		/* Physical page id of next page to write. */	
	id_t 	numWrites;				/* Number of page writes */
	id_t 	numOverWrites;			/* Number of page overwrites */
	id_t 	numReads;				/* Number of page reads */
	id_t 	bufferHits;				/* Number of pages returned from buffer rather than storage */
	count_t lastHit;				/* Buffer id of last buffer page hit */
	count_t nextBufferPage;			/* Next page buffer id to use. Round robin */
	id_t* 	activePath;				/* Active path on insert. Also contains root. Helps to prioritize. */
	void	*state;					/* Tree state */
	int8_t (*isValid)(void *state, id_t pageNum, id_t *parentId, void **parentBuffer);	/* Function to determine if page is valid */
	int8_t (*checkMapping)(void *state, id_t pageNum);									/* Function to determine if have space to add mapping with given pageNum */
	int8_t 	(*movePage)(void *state, id_t prev, id_t curr, void* buf);					/* Function called when buffer moves a page location */
	bitarr freePages;				/* Bit vector to determine free pages in memory */
} dbbuffer;

/**
@brief     	Initializes buffer given page size and number of pages.
@param     	state
                DBbuffer state structure
*/
void dbbufferInit(dbbuffer *state);

/**
@brief     	Initializes buffer and recovers previous state from storage.
@param     	state
                DBbuffer state structure
*/
void dbbufferRecover(dbbuffer *state);

/**
@brief      Reads page either from buffer or from storage. Returns pointer to buffer if success.
@param     	state
                DBbuffer state structure
@param     	pageNum
                Physical page id (number)
@return		Returns pointer to buffer page or NULL if error.
*/
void* readPage(dbbuffer *state, id_t pageNum);

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
void* readPageBuffer(dbbuffer *state, id_t pageNum, count_t bufferNum);

/**
@brief      Writes page to storage. Returns physical page id if success. -1 if failure.
@param     	state
                DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@return		
*/
int32_t writePage(dbbuffer *state, void* buffer);

/**
@brief      Overwrites page to storage at same physical address. -1 if failure.
			Caller is responsible for knowing that overwrite is possible given page contents.
@param     	state
                DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@return		
*/
int32_t overWritePage(dbbuffer *state, void* buffer, int32_t pageNum);

/**
@brief     	Initialize in-memory buffer page.
@param     	state
                DBbuffer state structure
@param     	pageNum
                In memory buffer page id (number)
@return		pointer to initialized page
*/
void* initBufferPage(dbbuffer *state, int pageNum);

/**
@brief     	Closes buffer.
@param     	state
                DBbuffer state structure
*/
void closeBuffer(dbbuffer *state);

/**
@brief     	Prints statistics.
@param     	state
                DBbuffer state structure
*/
void printStats(dbbuffer *state);

/**
@brief      Erases physical pages start to end inclusive. Assumes that start and end are aligned according to erase block.
@param     	state
               	DBbuffer state structure
@param     	startPage
                Physical index of start page
@param     	endPage
				Physical index of start page
@return		Return 0 if success, -1 if failure.
*/
int8_t erasePages(dbbuffer *state, id_t startPage, id_t endPage);

/**
@brief     	Clears statistics.
@param     	state
                DBbuffer state structure
*/
void dbbufferClearStats(dbbuffer *state);

/**
@brief     	Set page as free.
@param     	state
                DBbuffer state structure
@param     	pageNum
				Physical index of page				
*/
void dbbufferSetFree(dbbuffer *state, id_t pageNum);

/**
@brief     	Set page as valid (used).
@param     	state
                DBbuffer state structure
@param     	pageNum
				Physical index of page				
*/
void dbbufferSetValid(dbbuffer *state, id_t pageNum);

/**
@brief     	Returns 1 if page is free (can be used), 0 otherwise.
@param     	state
                DBbuffer state structure
@param     	pageNum
				Physical index of page				
*/
int8_t dbbufferIsFree(dbbuffer *state, id_t pageNum);

/**
@brief     	Returns 1 if have freed sufficient space up to requested number of pages, 0 otherwise.
@param     	state
                DBbuffer state structure
@param     	pages
				Number of free pages required				
*/
int8_t dbbufferEnsureSpace(dbbuffer *state, count_t pages);

#endif
