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
#include "randomseq.h"
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

/**
 * Runs all tests and collects benchmarks
 */ 
void runalltests_vmtree(memory_t* storageInfo)
{    
    printf("\nSTARTING VMTREE INDEX TESTS.\n");

    uint32_t stepSize = 100, numSteps = 10;
    int32_t numRecords = 10000;
    count_t r, numRuns = 3, l;
    uint32_t times[numSteps][numRuns];
    uint32_t reads[numSteps][numRuns];
    uint32_t writes[numSteps][numRuns];
    uint32_t overwrites[numSteps][numRuns];
    uint32_t hits[numSteps][numRuns];    
    uint32_t rtimes[numSteps][numRuns];
    uint32_t rreads[numSteps][numRuns];
    uint32_t rhits[numSteps][numRuns];

    int8_t M = 3, logBufferPages = 2;    
    int8_t  rnddata = 0;
    SD_FILE    *infile;

    if (!rnddata)
    {   /* Open file to read input records */
    
        infile = fopen("data/sea100K.bin", "r+b"); 
        numRecords = 10000;   
                
        /*
        infile = fopen("data/uwa500K.bin", "r+b");
        minRange = 946713600;
        maxRange = 977144040;
        numRecords = 500000;
        */
        stepSize = numRecords / numSteps;
    }

    for (r=0; r < numRuns; r++)
    {
        uint32_t errors = 0;
        uint32_t i;

        srand(r);
        randomseqState rnd;
        rnd.size = numRecords;
        stepSize = rnd.size / numSteps;
        uint32_t n = rnd.size; 
        rnd.prime = 0;        

        /* Configure file storage */      
        /*   
        printf("Using SD card file storage\n");    
        fileStorageState *storage = (fileStorageState*) malloc(sizeof(fileStorageState));
        storage->fileName = (char*) "myfile.bin";
        storage->storage.size = 5000;
        if (fileStorageInit((storageState*) storage) != 0)
        {
            printf("Error: Cannot initialize storage!\n");
            return;
        }                        
        */
        /* Configure dataflash memory storage */     
        
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
        if (buffer->blockBuffer == NULL)
        {   printf("Failed to allocate block buffer.\n");
            return;
        }
        buffer->storage = (storageState*) storage;         

        /* Configure btree state */
        vmtreeState* state = (vmtreeState*) malloc(sizeof(vmtreeState));
        if (state == NULL)
        {   printf("Failed to allocate VMtree state struct.\n");
            return;
        }
        if (!rnddata)
        {
            state->recordSize = 8;
            state->keySize = 8;
            state->dataSize = 0; 
        }
        else
        {
            state->recordSize = 16;
            state->keySize = 4;
            state->dataSize = 12;       
        }  
        state->buffer = buffer;
        
        state->tempKey = malloc(state->keySize); 
        state->tempKey2 = malloc(state->keySize); 
        int16_t ds = state->dataSize;
        if (ds < state->keySize)
            ds = state->keySize;
        state->tempData = malloc(ds);           	               

        state->mappingBufferSize = 1024;
        state->mappingBuffer = malloc(state->mappingBufferSize);
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
        buffer->checkMapping = vmtreeCheckMappingSpace;

        // state->parameters = NOR_OVERWRITE;      /* TODO: Set to OVERWRITE to enable overwrite, or NOR_OVERWRITE. */
        state->parameters = 0;
        // state->parameters = OVERWRITE;

        if (state->parameters == 0)
            printf("VMTREE with no overwrite.\n");
        else if (state->parameters == OVERWRITE)
            printf("VMTREE with overwrite.\n");
        else if (state->parameters == NOR_OVERWRITE)
            printf("VMTREE with NOR overwrite.\n");
        printf("Storage size: %lu  Memory size: %lu\n", storage->storage.size, M);

        /* Initialize VMTree structure with parameters */
        vmtreeInit(state);
        if (!rnddata)
            state->compareKey = compareIdx;

        int8_t* recordBuffer = (int8_t*) malloc(state->recordSize);  
        /* Data record is empty. Only need to reset to 0 once as reusing struct. */        
        for (i = 0; i < (uint16_t) (state->recordSize-4); i++) // 4 is the size of the key
        {
            recordBuffer[i + sizeof(int32_t)] = 0;
        }

        unsigned long start = millis();   
    
        if (rnddata)
        { 
            srand(r);
            randomseqInit(&rnd);
                
            for (i = 1; i <= n ; i++)
            {                       
                id_t v = randomseqNext(&rnd);               

                *((int32_t*) recordBuffer) = v;
                *((int32_t*) (recordBuffer+4)) = v;             
                // printf("Num: %lu KEY: %lu\n", i, v);

                /*
                if (i == 1000)
                {
                    // printf("Num: %lu KEY: %lu\n", i, v);
                    vmtreePrint(state);   
                //    vmtreePrintMappings(state);
                }
                */

                if (vmtreePut(state, recordBuffer, (void*) (recordBuffer + 4)) == -1)
                {    
                    printf("INSERT ERROR: %lu\n", v);              
                    vmtreePrint(state);     
                    vmtreePrintMappings(state);                      
                    return;
                }
                
                int32_t errors = 0;
                /*
                if (i != 1 && (i-1) % 64 == 0)
                {                
                    int32_t errors = checkValues(state, recordBuffer, rnd.size, i-1, r);
                    
                    if (errors > 0)
                    {
                        printf("ERRORS: %d Num: %d\n", errors, i);
                        vmtreePrint(state);   
                        vmtreePrintMappings(state);
                        return;
                    }
                }
                */     
                
                if (i % stepSize == 0)
                {           
                    printf("Num: %lu KEY: %lu\n", i, v);
                    // btreePrint(state);               
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
        }
        else
        {
            start = clock();                         
        
            /* Read data from a file */
            char infileBuffer[512];
            char record[16];
            int8_t headerSize = 16, recordSize = 16;
            i = 0;
            fseek(infile, 0, SEEK_SET);

            while (1)
            {
                /* Read page */
                if (0 == fread(infileBuffer, buffer->pageSize, 1, infile))
                    break;
                        
                /* Process all records on page */
                int16_t count = *((int16_t*) (infileBuffer+4));                  
                for (int j=0; j < count; j++)
                {	
                    void *buf = (infileBuffer + headerSize + j*recordSize);				
                              
                    /* Insert secondary index record (dataValue, recordNum) into B-tree secondary index */
			        memcpy(record, (void*) (buf + 4), sizeof(id_t));
                    memcpy((void*) (record + 4), &i, sizeof(id_t));			                     

                    // printf("Num: %d KEY: %d - %d\n", i, *((int32_t*) record), *((int32_t*) (record+4)));                        

                    if (vmtreePut(state, &record, &record) == -1)
                    {  
                        vmtreePrint(state);   
                        vmtreePrintMappings(state);
                        printf("INSERT ERROR: %d\n",  *((int32_t*) (recordBuffer+4)));
                        return;
                    }                                                                 

                    if (i % stepSize == 0)
                    {           
                        printf("Num: %d KEY: %d - %d\n", i, *((int32_t*) record), *((int32_t*) (record+4)));                   
                        l = i / stepSize -1;
                        if (l < numSteps && l >= 0)
                        {
                            times[l][r] = (clock()-start)*1000/CLOCKS_PER_SEC;
                            reads[l][r] = state->buffer->numReads;
                            writes[l][r] = state->buffer->numWrites;
                            overwrites[l][r] = state->buffer->numOverWrites;                     
                            hits[l][r] = state->buffer->bufferHits;                       
                        }
                    }  
                    i++;  
                    if (i == numRecords)
                        goto doneread;
                }
            }  
    doneread:
            numRecords = i;                
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

        // vmtreePrintMappings(state);
        // vmtreePrint(state);             
        printStats(state->buffer);

        printf("Elapsed Time: %lu s\n", (end - start));
        printf("Records inserted: %lu\n", n);
        printf("Mapping comparisons: %lu\n", state->numMappingCompare);

        /* Re-write tree to remove all mappings */
        // printf("Before clear mappings\n");
        /* OPTIONAL: Re-write tree to remove all mappings */
        // vmtreeClearMappings(state, state->activePath[0]);
        // printf("After clear mappings\n");

        state->numMappingCompare = 0;
        dbbufferClearStats(state->buffer);

        printf("\nVerifying and searching for all values.\n");
        start = millis();

        if (rnddata)
        {
            srand(1);
            randomseqInit(&rnd);

            /* Verify that can find all values inserted */    
            for (i = 1; i <= n; i++) 
            { 
                int32_t key = randomseqNext(&rnd);
                int8_t result = vmtreeGet(state, &key, recordBuffer);
                if (result != 0) 
                {   errors++;
                    printf("ERROR: Failed to find: %lu\n", key);
                    vmtreeGet(state, &key, recordBuffer);
                }
                else if (*((int32_t*) recordBuffer) != key)
                {   printf("ERROR: Wrong data for: %lu\n", key);
                    printf("Key: %lu Data: %lu\n", key, *((int32_t*) recordBuffer));
                }

                if (i % stepSize == 0)
                {                           
                    // btreePrint(state);               
                    l = i / stepSize - 1;
                    if (l < numSteps && l >= 0)
                    {
                        rtimes[l][r] = millis()-start;
                        rreads[l][r] = state->buffer->numReads;                    
                        rhits[l][r] = state->buffer->bufferHits;                     
                    }
                }    
            }
        }
        else
        {
            /* Read data from a file */
            char infileBuffer[512];
            char record[16];
             int8_t headerSize = 16, recordSize = 16;
            i = 0;
            fseek(infile, 0, SEEK_SET);

            while (1)
            {
                /* Read page */
                if (0 == fread(infileBuffer, buffer->pageSize, 1, infile))
                    break;
                        
                /* Process all records on page */
                int16_t count = *((int16_t*) (infileBuffer+4));                  
                for (int j=0; j < count; j++)
                {	
                    void *buf = (infileBuffer + headerSize + j*recordSize);				
                              
                    /* Get secondary index record (dataValue, recordNum) into B-tree secondary index */
			        memcpy(record, (void*) (buf + 4), sizeof(id_t));
                    memcpy((void*) (record + 4), &i, sizeof(id_t));			                     

                    // printf("Num: %d KEY: %d - %d\n", i, *((int32_t*) record), *((int32_t*) (record+4)));     

                    int8_t result = vmtreeGet(state, record, recordBuffer);
                    if (result != 0) 
                    {   errors++;
                        printf("ERROR: Failed to find: %d - %d\n", *((int32_t*) record), *((int32_t*) (record+4)));
                        vmtreePrint(state); 
                        vmtreeGet(state, record, recordBuffer);
                    }                           

                    if (i % stepSize == 0)
                    {           
                        printf("Num: %d KEY: %d - %d\n", i, *((int32_t*) record), *((int32_t*) (record+4)));       
                        l = i / stepSize -1;
                        if (l < numSteps && l >= 0)
                        {
                            rtimes[l][r] = (clock()-start)*1000/CLOCKS_PER_SEC;
                            rreads[l][r] = state->buffer->numReads;                    
                            rhits[l][r] = state->buffer->bufferHits;                          
                        }
                    }  
                    i++;  
                     if (i == numRecords)
                        goto donequery;
                }
            }   
donequery:
                numRecords = i;     
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
        printf("Mapping comparisons: %lu\n", state->numMappingCompare);

        /* Optional: Test iterator */
        // testIterator(state, recordBuffer);
        // printStats(buffer);

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

