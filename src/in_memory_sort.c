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

#include <string.h>
#include <stdlib.h>

#include "in_memory_sort.h"

/**
@brief     	Swaps two records.
@param     	tmp_buffer
                Temporary buffer
@param     	value_size
                Size of each record
@param		a
				First record
@param		b
				Second record
*/
void in_memory_swap(void *tmp_buffer, int value_size, char *a, char *b) 
{
	memcpy(tmp_buffer, a, value_size);
	memcpy(a, b, value_size);
	memcpy(b, tmp_buffer, value_size);
}

/**
@brief     	Performs quick sort partition.
@param     	tmp_buffer
                Temporary buffer
@param     	value_size
                Size of each record
@param		compare_fcn
				Comparison function
@param		low
				Low value
@param		high
				High value
@param		offset
				Pointer offset when performing comparison (allows to compare not just at start of record)
@return		Pointer to high value.
*/
void*
in_memory_quick_sort_partition(void *tmp_buffer, int value_size, int8_t (*compare_fcn)(void* a, void* b),
										char* low, char* high, int16_t offset) 
{
	char* pivot	= low;
	char* lower_bound	= low - value_size;
	char* upper_bound	= high + value_size;

	while (1) {
		do {
			upper_bound -= value_size;
		} while (compare_fcn(upper_bound+offset, pivot+offset) > 0);

		do {
			lower_bound += value_size;
		} while (compare_fcn(lower_bound+offset, pivot+offset) < 0);

		if (lower_bound < upper_bound) {
			in_memory_swap(tmp_buffer, value_size, lower_bound, upper_bound);
		}
		else {
			return upper_bound;
		}
	}
}

/**
@brief     	Performs quick sort.
@param     	tmp_buffer
                Temporary buffer
@param     	num_values
                Number of records
@param     	value_size
                Size of each record
@param		compare_fcn
				Comparison function
@param		low
				Low value
@param		high
				High value
@param		offset
				Pointer offset when performing comparison (allows to compare not just at start of record)
@return		Pointer to high value.
*/
void
in_memory_quick_sort_helper(void *tmp_buffer, uint32_t num_values, int value_size, int8_t (*compare_fcn)(void* a, void* b),
								char* low, char* high, int16_t offset) 
{
	if (low < high) 
	{
		char* pivot = in_memory_quick_sort_partition(tmp_buffer, value_size, compare_fcn, low, high, offset);

		in_memory_quick_sort_helper(tmp_buffer, num_values, value_size, compare_fcn, low, pivot, offset);
		in_memory_quick_sort_helper(tmp_buffer, num_values, value_size, compare_fcn, pivot + value_size, high, offset);
	}
}

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
int in_memory_sort(void *data, uint32_t num_values, int16_t value_size, int8_t (*compare_fcn)(void* a, void* b), int16_t offset)
{
	void* tmp_buffer = malloc(value_size);
	if	(NULL == tmp_buffer) 
		return 8;

	/*void* low = data*/
	char* high = (char*)data + (num_values-1)*value_size;
	in_memory_quick_sort_helper(tmp_buffer, num_values, value_size, compare_fcn, (char*)data, high, offset);

	free(tmp_buffer);

	return 0;
}
