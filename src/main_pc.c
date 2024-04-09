/******************************************************************************/
/**
@file		    main_pc.c
@author		  Ramon Lawrence, Scott Fazackerley
@brief		  Main program for testing VMtree for performance results on a PC.
@copyright	Copyright 2024
			      The University of British Columbia
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

#include "test_vmtree.h"

/**
 * Main function to run tests
 */ 
void main()
{
  int16_t M = 3, logBufferPages = 0, numRuns = 3;
  int8_t type = VMTREE;   // VMTREE, BTREE, OVERWRITE
  int8_t testType = 0;    // 0 - random, 1 - SeaTac, 2 - UWA, 3 - health, 4 - health (text)
                          // 5 - storage performance test

  recordIteratorState* it  = NULL;

  switch (testType)
  {
    case 0:
      it = randomIterator(100000);
      runtestpc(M, logBufferPages, numRuns, 16, 4, 12, type, it, uint32Compare);
      break;

    case 1: 
      it = fileIterator(100000, (char*) "data/sea100K.bin", 4, 16);   // Offset 4: temp, 8: pressure, 12: wind
      runtestpc(M, logBufferPages, numRuns, 8, 8, 0, type, it, compareIdx);
      break;

    case 2: 
      it = fileIterator(100000, (char*) "data/uwa500K.bin", 4, 16); // Offset 4: temp, 8: pressure, 12: wind
      runtestpc(M, logBufferPages, numRuns, 8, 8, 0, type, it, compareIdx);
      break;

    case 3:
      it = fileIterator(100000, (char*) "data/S7hl500K.bin", 0, 32);  
      runtestpc(M, logBufferPages, numRuns, 8, 8, 0, type, it, compareIdx);
      break;

    case 4:
      it = textIterator(100000, (char*) "data/S7_respiban_500K.txt", 3, (char*) "\t", 2, -1);  
      runtestpc(M, logBufferPages, numRuns, 8, 8, 0, type, it, compareIdx);
      break;

    case 5:
      testRawPerformanceFileStorage();
      break;
  } 

  if (it != NULL)  
    free(it);  
}  
