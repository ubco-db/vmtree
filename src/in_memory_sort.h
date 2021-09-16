/******************************************************************************/
/**
@file
@author		Kris Wallperington
@author		Ramon Lawrence
@brief		Implementation of an in-place, recursive quicksort written by the author.
@copyright	Copyright 2021
			The University of British Columbia,
			IonDB Project Contributors (see AUTHORS.md)
@par
			Licensed under the Apache License, Version 2.0 (the "License");
			you may not use this file except in compliance with the License.
			You may obtain a copy of the License at
			http://www.apache.org/licenses/LICENSE-2.0
@par
			Unless required by applicable law or agreed to in writing,
			software distributed under the License is distributed on an
			"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
			either express or implied. See the License for the specific
			language governing permissions and limitations under the
			License.
*/
/******************************************************************************/

#ifndef IN_MEMORY_SORT_H
#define IN_MEMORY_SORT_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

/**
@brief     	Performs in-memory quick sort for data records given a comparison function.
@param     	data
                Pointer to memory storing records
@param     	num_values
                Number of records
@param     	value_size
                Size of each record
@param		compare_fcn
				Comparison function
@param		offset
				Pointer offset when performing comparison (allows to compare not just at start of record)
@return		Return 0 if success. Non-zero value if error.
*/
int in_memory_sort(void *data, uint32_t num_values, int16_t value_size, int8_t (*compare_fcn)(void* a, void* b), int16_t offset);

#if defined(__cplusplus)
}
#endif

#endif
