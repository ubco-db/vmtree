/******************************************************************************/
/**
@file		fileStorage.c
@author		Ramon Lawrence
@brief		File storage implementation for reading and writing pages of data.
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

#include "fileStorage.h"
#include <string.h>

/**
@brief     	Initializes storage. Opens file.
@param		state
                File storage state structure
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t fileStorageInit(storageState *storage)
{	 
	fileStorageState *fs = (fileStorageState*) storage;

	#ifndef MULTIFILE
	// Single-file implementation
	char str[20];
	sprintf(str, "%s.bin", fs->fileName);
	fs->file = fopen(str, "w+b");
    if (NULL == fs->file) 
		return -1;
	#else
	// Multi-file implementation	
	char str[20];
	
	for (uint8_t i=0; i < NUM_FILES; i++)
	{
		sprintf(str, "%s%d.bin", fs->fileName, i);		
		// printf("%s\n", str);		
		fs->files[i] = fopen(str, "w+b");				
    	if (NULL == fs->files[i]) 
			return -1;
	}
	#endif

	fs->storage.init = fileStorageInit;
	fs->storage.close = fileStorageClose;
	fs->storage.readPage = fileStorageReadPage;
	fs->storage.writePage = fileStorageWritePage;
	fs->storage.erasePages = fileStorageErasePages;
	fs->storage.flush = fileStorageFlush;

	return 0;	
}

/**
@brief      Retrieves file handle for file that should contain given page number.
@param     	state
                File storage state structure
@param     	pageNum
                Physical page id (number)
@return		 Returns file handle. May change pageNum to be proper offset in file.
*/
SD_FILE*  getFile(fileStorageState *fs, id_t* pageNum)
{	
	#ifndef MULTIFILE
	// Single-file implementation
	return fs->file;
	#else
	// Multi-file implementation
	uint8_t idx = *pageNum / fs->fileSize;	
	if (idx >= NUM_FILES)
		idx = NUM_FILES - 1;	
	*pageNum = *pageNum % fs->fileSize;
	// printf("Index: %d pageNum: %d\n", idx, *pageNum);
	return fs->files[idx];
	#endif
}

/**
@brief      Reads page from storage into buffer. Returns 0 if success, non-zero if failure.
@param     	state
                File storage state structure
@param     	pageNum
                Physical page id (number)
@param		pageSize
				Size of page to read in bytes
@param		buffer
				Pointer to buffer to copy data into
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t fileStorageReadPage(storageState *storage, id_t pageNum, count_t pageSize, void *buffer)
{	
	fileStorageState *fs = (fileStorageState*) storage;

	SD_FILE* fp = getFile(fs, &pageNum);	

    /* Seek to page location in file */
    if (fseek(fp, pageNum*pageSize, SEEK_SET) == -1)
		return -1;

    /* Read page into start of buffer 1 */  
	int8_t result = fread(buffer, pageSize, 1, fp);
    if (result == 0)
		return -1;           
	   
	return 0;
}

/**
@brief      Writes page from buffer into storage. Returns 0 if success, non-zero if failure.
@param     	state
                File storage state structure
@param     	pageNum
                Physical page id (number)
@param		pageSize
				Size of page to write in bytes
@param		buffer
				Pointer to buffer to copy data into
@return		 Returns 0 if success, non-zero if failure.
*/
int8_t fileStorageWritePage(storageState *storage, id_t pageNum, count_t pageSize, void *buffer)
{    
	fileStorageState *fs = (fileStorageState*) storage;

	SD_FILE* fp = getFile(fs, &pageNum);

	/* Seek to page location in file */
    if (fseek(fp, pageNum*pageSize, SEEK_SET) == -1)
		return -1;

	int16_t result = fwrite(buffer, pageSize, 1, fp);
	// printf("Write page: %d size: %d\n", pageNum, result);
	if (result != 0)
		return -1;

	return 0;
}

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
int8_t fileStorageErasePages(storageState *storage, id_t startPage, id_t endPage)
{
	/* Nothing to do */
	return 0;
}

/**
@brief     	Flush storage and ensure all data is written.
@param     	state
                File storage state structure
*/
void fileStorageFlush(storageState *storage)
{
	fileStorageState *fs = (fileStorageState*) storage;
	
	#ifndef MULTIFILE
	// Single-file implementation
	fflush(fs->file);
	#else	
	// Multi-file implementation
	for (uint8_t i=0; i < NUM_FILES; i++)
	{
		fflush(fs->files[i]);		
	}
	#endif
}


/**
@brief     	Closes storage and performs any needed cleanup.
@param     	state
                File storage state structure
*/
void fileStorageClose(storageState *storage)
{	
	fileStorageState *fs = (fileStorageState*) storage;
	
	#ifndef MULTIFILE
	// Single-file implementation
	 fclose(fs->file);
	#else	
	// Multi-file implementation
	for (uint8_t i=0; i < NUM_FILES; i++)
	{
		fclose(fs->files[i]);		
	}	
	#endif
}

