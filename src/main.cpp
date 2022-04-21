/******************************************************************************/
/**
@file		    main.cpp
@author		  Ramon Lawrence, Scott Fazackerley
@brief		  Main program for testing VMtree on custom hardware.
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

#include "Arduino.h"
#include "SPI.h"

/**
 * SPI configurations for memory */
#include "mem_spi.h"

/*
Includes for DataFlash memory
*/
#include "dataflash.h"

/**
 * Includes for SD card 
*/
/** @TODO optimize for clock speed */
#include "sdios.h"
static ArduinoOutStream cout(Serial);

#include "SdFat.h"
#include "sd_test.h"

#include "test_vmtree.h"
#include "file/dataflash_c_iface.h"

#define ENABLE_DEDICATED_SPI 1
#define SPI_DRIVER_SELECT 1
// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 1
/** @TODO Update max SPI speed for SD card */
#define SD_CONFIG SdSpiConfig(CS_SD, DEDICATED_SPI, SD_SCK_MHZ(12), &spi_0)

SdFat32 sd;
File32 file;

// Headers
bool test_sd_card();

recordIteratorState* randomIterator(int32_t numRecords)
{
    randomIteratorState* iter = (randomIteratorState*) malloc(sizeof(randomIteratorState));
    randomIteratorInit((recordIteratorState*) iter);

    iter->state.size = numRecords;
    return (recordIteratorState*) iter;
}

recordIteratorState* fileIterator(int32_t numRecords, char* fileName)
{                            
    fileIteratorState* iter = (fileIteratorState*) malloc(sizeof(fileIteratorState));      
    iter->filePath = fileName;  
    iter->file = NULL;      
    iter->pageSize = 512;
    iter->recordSize = 16;
    iter->headerSize = 16;        
    iter->buffer = (char*) malloc(iter->pageSize);
    fileIteratorInit((recordIteratorState*) iter);

    iter->state.size = numRecords;
    return (recordIteratorState*) iter;
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
  {
    delay(1);
  }

  delay(1000);  

  pinMode(CHK_LED, OUTPUT);
  pinMode(PULSE_LED, OUTPUT);

  /** example for SD card access */
  Serial.print("\nInitializing SD card...");
  if (test_sd_card())
  {
    file = sd.open("/");
    cout << F("\nList of files on the SD.\n");
    sd.ls("/", LS_R);
  }
 
  init_sdcard((void*) &sd);

  /* Setup for data flash memory (DB32 512 byte pages) */
  pinMode(CS_DB32,  OUTPUT);
  digitalWrite(CS_DB32, HIGH);
  at45db32_m.spi->begin();

  df_initialize(&at45db32_m);
  cout << "AT45DF32" << "\n";
  cout << "page size: " << (at45db32_m.actual_page_size = get_page_size(&at45db32_m)) << "\n";
  cout << "status: " << get_ready_status(&at45db32_m) << "\n";
  cout << "page size: " << (at45db32_m.actual_page_size) << "\n";
  at45db32_m.bits_per_page = (uint8_t)ceil(log2(at45db32_m.actual_page_size));
  cout << "bits per page: " << (unsigned int)at45db32_m.bits_per_page << "\n";

  init_df((void*) &at45db32_m);

  int16_t M = 3, logBufferPages = 2, numRuns = 3;
  int8_t type = OVERWRITE;   // VMTREE, BTREE, OVERWRITE
  recordIteratorState* it;

  // it = randomIterator(10000);
  // runtest(&at45db32_m, M, logBufferPages, numRuns, 16, 4, 12, type, it, uint32Compare);
  // free(it);


  it = fileIterator(10000, (char*) "sea100K.bin");
  // recordIteratorState* it = fileIterator(100000, "data/uwa500K.bin");
  runtest(&at45db32_m, M, logBufferPages, numRuns, 8, 8, 0, type, it, compareIdx);
  free(it);
}

void loop() {
  digitalWrite(CHK_LED, HIGH);
  digitalWrite(PULSE_LED, HIGH);

  delay(1000);
  digitalWrite(CHK_LED, LOW);
  digitalWrite(PULSE_LED, LOW);
  delay(1000); 
}

/**
 * Testing for SD card -> Can be removed as needed */
bool test_sd_card()
{
  if (!sd.cardBegin(SD_CONFIG))
  {
    Serial.println(F(
        "\nSD initialization failed.\n"
        "Do not reformat the card!\n"
        "Is the card correctly inserted?\n"
        "Is there a wiring/soldering problem?\n"));
    if (isSpi(SD_CONFIG))
    {
      Serial.println(F(
          "Is SD_CS_PIN set to the correct value?\n"
          "Does another SPI device need to be disabled?\n"));
    }
    errorPrint(sd);
    return false;
  }

  if (!sd.card()->readCID(&m_cid) ||
      !sd.card()->readCSD(&m_csd) ||
      !sd.card()->readOCR(&m_ocr))
  {
    cout << F("readInfo failed\n");
    errorPrint(sd);
  }
  printCardType(sd);
  cidDmp();
  csdDmp();
  cout << F("\nOCR: ") << uppercase << showbase;
  cout << hex << m_ocr << dec << endl;
  if (!mbrDmp(sd))
  {
    return false;
  }
  if (!sd.volumeBegin())
  {
    cout << F("\nvolumeBegin failed. Is the card formatted?\n");
    errorPrint(sd);
    return false;
  }
  dmpVol(sd);
  return true;
}