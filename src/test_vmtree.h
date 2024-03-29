/******************************************************************************/
/**
@file		test_vmtree.h
@author		Ramon Lawrence
@brief		This file does performance/correctness testing of VMTree.
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
#include <time.h>
#include <string.h>

#include "vmtree.h"
#include "testIterators/randomIterator.h"
#include "testIterators/fileIterator.h"
#include "testIterators/textIterator.h"
#include "fileStorage.h"
#include "dfStorage.h"
// #include "memStorage.h"


void testIterator(vmtreeState *state, void *recordBuffer)
{
     /* Below minimum key search */
    int32_t key = -1;
    int8_t result = vmtreeGet(state, &key, recordBuffer);
    if (result == 0) 
        printf("Error1: Key found: %li\n", key);

    /* Above maximum key search */
    key = 3500000;
    result = vmtreeGet(state, &key, recordBuffer);
    if (result == 0) 
        printf("Error2: Key found: %li\n", key);
    
    free(recordBuffer);
    
    vmtreeIterator it;
    uint32_t mv = 40;     // For all records, select mv = 1.
    it.minKey = &mv;
    int v = 299;
    it.maxKey = &v;           

    vmtreeInitIterator(state, &it);
    uint32_t i = 0;
    int8_t success = 1;    
    uint32_t *itKey, *itData;

    while (vmtreeNext(state, &it, (void**) &itKey, (void**) &itData))
    {                      
        // printf("Key: %d  Data: %d\n", *itKey, *itData);
        if (i+mv != *itKey)
        {   success = 0;
            printf("Key: %lu Error\n", *itKey);
        }
        i++;        
    }
    printf("\nRead records: %lu\n", i);

    if (success && i == (v-mv+1))
        printf("SUCCESS\n");
    else
        printf("FAILURE\n");           
}

int32_t checkValues(vmtreeState *state, void* recordBuffer, int32_t maxvals, int numvals, int r)
{
     /* Verify that can find all values inserted */
    size_t errors = 0;
    randomseqState rnd;
    rnd.size = maxvals;
    rnd.prime = 0;
    srand(r);
    randomseqInit(&rnd);
        
    for (int32_t i = 0; i < numvals; i++) 
    {         
        int32_t key = randomseqNext(&rnd);         
        int8_t result = vmtreeGet(state, &key, recordBuffer);
        
        if (result != 0) 
        {   errors++;
            printf("ERROR: Failed to find: %d\n", key);
            vmtreeGet(state, &key, recordBuffer);
        }
        else if (*((int32_t*) recordBuffer) != key)
        {   printf("ERROR: Wrong data for: %d\n", key);
            printf("Key: %d Data: %d\n", key, *((int32_t*) recordBuffer));
            errors++;
        }
    }
    
    return errors;
}

void testRawPerformance()
{   /* Tests storage raw read and write performance */
    printf("Starting RAW performance test.\n");

    char buffer[512];
    unsigned long startMillis;
    /* SD Card */    
    SD_FILE *fp = fopen("testdata.bin", "w+b");
    if (fp == NULL)
    {
        printf("Error opening file.\n");
        return;
    }
   
    // Test time to write 1000 blocks
    printf("SD card performance metrics:\n");
    startMillis = millis();

    for (int i=0; i < 1000; i++)
    {            
        if (0 == fwrite(buffer, 512, 1, fp))
        {   printf("Write error.\n");             
        }
    }
    printf("Write time: %lu\n", millis()-startMillis);
    fflush(fp);

    startMillis = millis();

    for (int i=0; i < 1000; i++)
    {            
        unsigned long num = rand() % 1000;         
        fseek(fp, num*512 , SEEK_SET);
        if (0 == fwrite(buffer, 512, 1, fp))
        {   printf("Write error.\n");             
        }
    }
    printf("Random write time: %lu\n", millis()-startMillis);
    fflush(fp);

    // Time to read 1000 blocks
    fseek(fp, 0, SEEK_SET);
    startMillis = millis();
    for (int i=0; i < 1000; i++)
    {
        
        if (0 == fread(buffer, 512, 1, fp))
        {   printf("Read error.\n");             
        }
    }
    printf("Read time: %lu\n", millis()-startMillis);

    fseek(fp, 0, SEEK_SET);
    // Time to read 1000 blocks randomly    
    startMillis = millis();
    srand(1);
    for (int i=0; i < 1000; i++)
    {
        unsigned long num = rand() % 1000;         
        fseek(fp, num*512 , SEEK_SET);
        if (0 == fread(buffer, 512, 1, fp))
        {   printf("Read error.\n");             
        }
    }
    printf("Random Read time: %lu\n", millis()-startMillis);
   
	/* Data flash storage */
    printf("Dataflash performance metrics:\n");
    startMillis = millis();

    for (int i=0; i < 1000; i++)
    {
        if (0 == dfwrite(i, buffer, 512))
        {   printf("Write error.\n");             
        }
    }
    printf("Write time: %lu\n", millis()-startMillis);    

    startMillis = millis();

    for (int i=0; i < 1000; i++)
    {            
        unsigned long num = rand() % 1000;                
        if (0 == dfwrite(num, buffer, 512))
        {   printf("Write error.\n");             
        }
    }
    printf("Random write time: %lu\n", millis()-startMillis);    

    // Time to read 1000 blocks        
    startMillis = millis();
    for (int i=0; i < 1000; i++)
    {        
        if (0 == dfread(i, buffer, 512))
        {   printf("Read error.\n");             
        }
    }
    printf("Read time: %lu\n", millis()-startMillis);
    
    // Time to read 1000 blocks randomly    
    startMillis = millis();
    srand(1);
    for (int i=0; i < 1000; i++)
    {
        unsigned long num = rand() % 1000;                 
        if (0 == dfread(num, buffer, 512))
        {   printf("Read error.\n");             
        }
    }
    printf("Random Read time: %lu\n", millis()-startMillis);	    
}


/**
 * Runs test with given parameters.
 */ 
void runtest(memory_t* storageInfo, int16_t M, int16_t logBufferPages, int8_t numRuns, uint8_t recordSize, uint8_t keySize, uint8_t dataSize, uint8_t type, recordIteratorState *it,  int8_t (*compareKey)(void *a, void *b))
{    
    // testRawPerformance();
    // return;

    uint32_t stepSize, numSteps = 10;
    int32_t numRecords = it->size;
    count_t r, l;
    uint32_t times[numSteps][numRuns];
    uint32_t reads[numSteps][numRuns];
    uint32_t writes[numSteps][numRuns];
    uint32_t overwrites[numSteps][numRuns];
    uint32_t hits[numSteps][numRuns];    
    uint32_t rtimes[numSteps][numRuns];
    uint32_t rreads[numSteps][numRuns];
    uint32_t rhits[numSteps][numRuns];       
    id_t recid;

    stepSize = numRecords / numSteps;

    for (r=0; r < numRuns; r++)
    {
        uint32_t errors = 0;
        uint32_t i;  
        
        stepSize = it->size / numSteps;
        uint32_t n = it->size;          

        /* Configure file storage */      
        
        printf("Using SD card file storage\n");    
        fileStorageState *storage = (fileStorageState*) malloc(sizeof(fileStorageState));
        printf("FS size: %d\n", sizeof(fileStorageState));
        storage->fileName = (char*) "afile";
        storage->storage.size = 2000;
        storage->fileSize = storage->storage.size / NUM_FILES;
        printf("Num files: %d  File size: %d\n", NUM_FILES, storage->fileSize);
        if (fileStorageInit((storageState*) storage) != 0)
        {
            printf("Error: Cannot initialize storage!\n");
            return;
        }                        
        
        /* Configure dataflash memory storage */     
        /*
        printf("Using data flash storage\n");   
        dfStorageState *storage = (dfStorageState*) malloc(sizeof(dfStorageState)); 
        storage->df = storageInfo;
        storage->size = 512 * 6700; // 6700 pages of 512 bytes each (configure based on memory) 
        storage->storage.size = 6700;
        storage->pageOffset = 0;              
        if (dfStorageInit((storageState*) storage) != 0)
        {
            printf("Error: Cannot initialize storage!\n");
            return;
        }        
        */
        /* Configure memory storage */
        /*
        memStorageState *storage = malloc(sizeof(memStorageState));        
        if (memStorageInit((storageState*) storage) != 0)
        {
            printf("Error: Cannot initialize storage!\n");
            return;
        }
        */

        /* Configure buffer */
        dbbuffer* buffer = (dbbuffer*) malloc(sizeof(dbbuffer));
        printf("DBBuffer size: %d\n", sizeof(dbbuffer));
        if (buffer == NULL)
        {   printf("Failed to allocate buffer struct.\n");
            return;
        }
        buffer->pageSize = 512;
        buffer->numPages = M;
        buffer->eraseSizeInPages = 8;
        buffer->status = (id_t*) malloc(sizeof(id_t)*M);
        if (buffer->status == NULL)
        {   printf("Failed to allocate buffer status array.\n");
            return;
        }
        
        buffer->buffer  = malloc((size_t) buffer->numPages * buffer->pageSize);   
        if (buffer->buffer == NULL)
        {   printf("Failed to allocate buffer.\n");
            return;
        }
        buffer->blockBuffer = malloc((size_t) buffer->eraseSizeInPages * buffer->pageSize);
        printf("Block buffer size FS size: %d\n", (size_t) buffer->eraseSizeInPages * buffer->pageSize);
        if (buffer->blockBuffer == NULL)
        {   printf("Failed to allocate block buffer.\n");
            return;
        }
        buffer->storage = (storageState*) storage;         

        /* Configure btree state */
        vmtreeState* state = (vmtreeState*) malloc(sizeof(vmtreeState));
        printf("VMTree state size: %d\n", sizeof(vmtreeState));
        if (state == NULL)
        {   printf("Failed to allocate VMtree state struct.\n");
            return;
        }

        state->recordSize = recordSize;
        state->keySize = keySize;
        state->dataSize = dataSize;
        
        state->buffer = buffer;
        
        state->tempKey = malloc(state->keySize); 
        state->tempKey2 = malloc(state->keySize); 
        int16_t ds = state->dataSize;
        if (ds < state->keySize)
            ds = state->keySize;
        state->tempData = malloc(ds);           	               

        state->mappingBufferSize = 1024; // 1024;
        state->mappingBuffer = malloc(state->mappingBufferSize);
        printf("Mapping buffer size: %d\n", state->mappingBufferSize);
        if (state->mappingBuffer == NULL)
        {   printf("Failed to allocate mapping buffer size: %d\n", state->mappingBufferSize);
            return;
        }

        /* Enable log buffer by allocating space or set to NULL for no log buffer */
        state->logBuffer = NULL;
        state->logBufferSize = logBufferPages * buffer->pageSize;
        if (state->logBufferSize > 0)
            state->logBuffer  = malloc(state->logBufferSize);  

        /* Connections between buffer and VMTree */
        buffer->activePath = state->activePath;
        buffer->state = state;
        buffer->isValid = vmtreeIsValid;
        buffer->movePage = vmtreeMovePage;        

        state->parameters = type;  
        
         if (state->parameters == 0)
            printf("VMTREE with sequential writing.\n");
        else if (state->parameters == BTREE)
            printf("BTREE with update-in-place writes.\n");
        else if (state->parameters == OVERWRITE)
            printf("VMTREE with memory-supported overwriting.\n");
        printf("Storage size: %lu  Memory size: %lu\n", storage->storage.size, M);

        /* Initialize VMTree structure with parameters */
        vmtreeInit(state);
        state->compareKey = compareKey;  

        int8_t* recordBuffer = (int8_t*) malloc(state->recordSize);  
        /* Data record is empty. Only need to reset to 0 once as reusing struct. */        
        for (i = 0; i < (uint16_t) (state->recordSize-4); i++) // 4 is the size of the key
        {
            recordBuffer[i + sizeof(int32_t)] = 0;
        }

        srand(r); 
        it->init(it);

        unsigned long start = millis();   
        
        for (i = 1; i <= n ; i++)
        {                             
            it->next(it, recordBuffer, (void*) (recordBuffer+state->keySize), &recid);
            id_t v =*((int32_t*) recordBuffer);
            /*
            if (state->keySize == 8)
                printf("Num: %u KEY: %u - %u\n", i, *((uint32_t*) recordBuffer), *((uint32_t*) (recordBuffer+4)));
            else
                printf("\nNum: %u KEY: %u\n", i, v);          
            */
            if (vmtreePut(state, recordBuffer, (void*) (recordBuffer + state->keySize)) == -1)
            {  
                vmtreePrint(state);   
                vmtreePrintMappings(state);
                printf("INSERT ERROR: %d\n", v);
                return;
            }           
            /*                           
            int32_t errors = checkValues(state, recordBuffer, n, i, r);
            if (errors > 0)
            {
                printf("ERRORS: %d Num: %d\n", errors, i);
                vmtreePrint(state);   
                vmtreePrintMappings(state);
                return;
            }                
            */                      
            if (i % stepSize == 0)
            {           
                printf("Num: %lu KEY: %lu   Extra writes: %d \tMapping table #: %d\n", i, v, state->numMappingWrite, state->numMappings);                            
                l = i / stepSize -1;                

                if (l < numSteps && l >= 0)
                {
                    times[l][r] = millis()-start;
                    reads[l][r] = state->buffer->numReads;
                    writes[l][r] = state->buffer->numWrites;
                    overwrites[l][r] = state->buffer->numOverWrites;
                    hits[l][r] = state->buffer->bufferHits;                     
                }
            }        
        }
       
        /* Call flush to make sure log buffer or any other buffers have been wrote to storage */
        vmtreeFlush(state);

        unsigned long end = millis();   

        l = numSteps-1;
        times[l][r] = end-start;
        reads[l][r] = state->buffer->numReads;
        writes[l][r] = state->buffer->numWrites;
        overwrites[l][r] = state->buffer->numOverWrites;
        hits[l][r] = state->buffer->bufferHits;                              

        it->close(it);
        // vmtreePrintMappings(state);
        // vmtreePrint(state);             
        printStats(state->buffer);

        printf("Elapsed Time: %lu s\n", (end - start));
        printf("Records inserted: %lu\n", n);
        printf("Mapping comparisons: %lu  Extra writes: %d \n", state->numMappingCompare, state->numMappingWrite);

        /* Re-write tree to remove all mappings */
        // printf("Before clear mappings\n");
        /* OPTIONAL: Re-write tree to remove all mappings */
        // vmtreeClearMappings(state, state->activePath[0]);
        // printf("After clear mappings\n");

        state->numMappingCompare = 0;
        state->numMappingWrite = 0;
        dbbufferClearStats(state->buffer);

        srand(r); 
        it->init(it);

        printf("\nVerifying and searching for all values.\n");
        start = millis();

        /* Verify that can find all values inserted */    
        for (i = 0; i < n; i++) 
        {                         
            it->next(it, recordBuffer, (void*) (recordBuffer+state->keySize), &recid);
            id_t key =*((int32_t*) recordBuffer);            

            int8_t result = vmtreeGet(state, recordBuffer, (void*) (recordBuffer+state->keySize));            
            if (result != 0) 
            {   errors++;                
                if (state->keySize == 8)
                    printf("ERROR: Failed to find: Num: %u KEY: %u - %u\n", i, *((uint32_t*) recordBuffer), *((uint32_t*) (recordBuffer+4)));
                else
                    printf("ERROR: Failed to find: Num: %u KEY: %u\n", i, *((uint32_t*) recordBuffer));    
                vmtreeGet(state, recordBuffer, (void*) (recordBuffer+state->keySize));
            }
            else if (state->dataSize > 0 && *((uint32_t*) (recordBuffer+state->keySize)) != key)
            {   printf("ERROR: Wrong data for: %u\n", key);
                printf("Key: %u Data: %u\n", key, *((uint32_t*) recordBuffer));
            }
            if (i % stepSize == 0)
            {                                     
                l = i / stepSize - 1;
                if (l < numSteps && l >= 0)
                {
                    rtimes[l][r] = millis()-start;
                    rreads[l][r] = state->buffer->numReads;                    
                    rhits[l][r] = state->buffer->bufferHits;                     
                }
            }    
        }
        
        l = numSteps-1;       
        rtimes[l][r] = millis()-start;
        rreads[l][r] = state->buffer->numReads;                    
        rhits[l][r] = state->buffer->bufferHits;                     
    
        if (errors > 0)
            printf("FAILURE: Errors: %lu\n", errors);
        else
            printf("SUCCESS. All values found!\n");
        
        end = millis();
        printf("Elapsed Time: %lu s\n", (end - start));
        printf("Records queried: %lu\n", n);   
        printStats(state->buffer);     
        printf("Mapping comparisons: %lu  Extra writes: %d \n", state->numMappingCompare, state->numMappingWrite);

        /* Optional: Test iterator */
        // testIterator(state, recordBuffer);
        // printStats(buffer);

        it->close(it); 

        /* Clean up and free memory */
        closeBuffer(buffer);    
        
        free(state->tempKey);
        free(state->tempData);
        free(recordBuffer);
        free(state->logBuffer);
        free(state->buffer->blockBuffer);
        free(buffer->status);
        free(state->buffer->buffer);
        free(buffer);
        free(state);
    }

    // Prints results
    uint32_t sum;
    for (count_t i=1; i <= numSteps; i++)
    {
        printf("Stats for %lu:\n", i*stepSize);
    
        printf("Reads:   ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += reads[i-1][r];
            printf("\t%lu", reads[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Writes: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += writes[i-1][r];
            printf("\t%lu", writes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Overwrites: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += overwrites[i-1][r];
            printf("\t%lu", overwrites[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("Totwrites: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += overwrites[i-1][r] + writes[i-1][r];
            printf("\t%lu", overwrites[i-1][r] + writes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Buffer hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += hits[i-1][r];
            printf("\t%lu", hits[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("Write Time: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += times[i-1][r];
            printf("\t%lu", times[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("R Time: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rtimes[i-1][r];
            printf("\t%lu", rtimes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("R Reads: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rreads[i-1][r];
            printf("\t%lu", rreads[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("R Buffer hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rhits[i-1][r];
            printf("\t%lu", rhits[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
    }
}

