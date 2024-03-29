/******************************************************************************/
/**
@file		dfStorage.h
@author		Ramon Lawrence
@brief		Dataflash Memory storage for reading and writing pages of data.
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
#ifndef DFSTORAGE_H
#define DFSTORAGE_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>

#include "storage.h"

#if defined(ARDUINO)
#include "file/serial_c_iface.h"
#include "file/dataflash_c_iface.h"
#endif

typedef struct {
	storageState 	storage;			/* Base struct defining read/write page functions */
	void			*df;				/* Dataflash info */	
	uint32_t		size;				/* Storage size in bytes */
	uint32_t		pageOffset;			/* Offset of first page */
	uint32_t		maxPageWrite;		/* The largest page number written. Used to track when need to erase before write. */
	uint8_t			useOverwrite;		/* 0 if regular write, 1 if using overwrite without erase */
} dfStorageState;


/**
@brief     	Initializes storage.
@param		state
                Dataflash Memory storage state structure
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t dfStorageInit(storageState *storage);


/**
@brief      Reads page from storage into buffer. Returns 0 if success, non-zero if failure.
@param     	state
                 Dataflash Memory storage state structure
@param     	pageNum
                Physical page id (number)
@param		pageSize
				Size of page to read in bytes
@param		buffer
				Pointer to buffer to copy data into
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t dfStorageReadPage(storageState *storage, id_t pageNum, count_t pageSize, void *buffer);


/**
@brief      Writes page from buffer into storage. Returns 0 if success, non-zero if failure.
@param     	state
                Dataflash Memory storage state structure
@param     	pageNum
                Physical page id (number)
@param		pageSize
				Size of page to write in bytes
@param		buffer
				Pointer to buffer to copy data into
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t dfStorageWritePage(storageState *storage, id_t pageNum, count_t pageSize, void *buffer);


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
int8_t dfStorageErasePages(storageState *storage, id_t startPage, id_t endPage);


/**
@brief     	Flush storage and ensure all data is written.
@param     	state
                Dataflash Memory storage state structure
*/
void dfStorageFlush(storageState *storage);


/**
@brief     	Closes storage and performs any needed cleanup.
@param     	state
                Dataflash Memory storage state structure
*/
void dfStorageClose(storageState *storage);

#if defined(__cplusplus)
}
#endif

#endif
