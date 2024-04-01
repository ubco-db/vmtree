# Efficient B-tree Implementation for Embedded Sensor Devices

The VMTree is an efficient B+-tree implementation for embedded sensor devices. It uses a few KBs of memory and supports multiple storage types include SD cards, NOR, and NAND. VMTree extends the previous B-tree implementation ([PC](https://github.com/ubco-db/btree), [Arduino/Embedded](https://github.com/ubco-db/btree_raw)), which only worked with file storage.

There are three implementation variants optimized for different storage types:

1. **B+-tree** - for SD card storage with a file system
2. **VMTree** - for raw NAND storage
3. **VMTree-OW** - for NOR and Dataflash storage supporting page overwriting

The B+-tree implementation has the following benefits:

1. Uses the minimum of two page buffers for performing all operations. For higher performance, at least three buffers is recommended.
2. No use of dynamic memory (i.e. malloc()). All memory is pre-allocated at creation of the tree.
3. Efficient insert (put) and query (get) of arbitrary fixed-size key-value data.
4. Support for iterator to traverse data in sorted order.
5. Easy to use and include in existing projects. 
6. Open source license. Free to use for commerical and open source projects.

## License
[![License](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)

# Getting Started

To use VMTree in projects, the following files are required regardless of implementation variant:

* `vmtree.h`, `vmtree.c` - implementation of B+-tree supporting fixed-size key-value records
* `storage.h` - the storage interface
* `dbbuffer.h`, `dbbuffer.c` - provides buffering of pages in memory
* `in_memory_sort.h`, `in_memory_sort.c` - for sorting keys within nodes
* `bitarr.h`, `bitarr.c` - bit array implementation
  
## Support Code Files (optional - depends on your environment)

* `serial_c_iface.h`, `serial_c_iface.cpp` - allows printf() on Arduino
* `sd_stdio_c_iface.h`, `sd_stdio_c_iface.h` - allows use of stdio file API (e.g. fopen())

## Storage Type

* **SD Card storage with files** (most common) - requires `sd_card_c_iface.h`, `sd_card_c_iface.cpp`, `fileStorage.h`, `fileStorage.c`, and [SdFAT library](https://github.com/greiman/SdFat)
* **Dataflash storage** - requires `dataflash_c_iface.h`, `dataflash_c_iface.cpp`, `dfStorage.h`, `dfStorage.c`, and [Dataflash library](https://github.com/ubco-db/Dataflash)

The main benchmark and testing file is **`test_vmtree.h`**. The main file is in **`main.cpp`**. This will need to be modified for your particular embedded platform.
Our development on embedded devices is done using Platform.io. 

## Usage

### Setup B-tree and Configure Memory

```c
/* Configure SD card file storage */        
fileStorageState *storage = (fileStorageState*) malloc(sizeof(fileStorageState));
storage->fileName = (char*) "dfile";
storage->storage.size = 5000;
storage->fileSize = storage->storage.size / NUM_FILES;
if (fileStorageInit((storageState*) storage) != 0) {
	printf("Error: Cannot initialize storage!\n");
	return;
}

/* Configure buffer */
dbbuffer* buffer = (dbbuffer*) malloc(sizeof(dbbuffer));
if (buffer == NULL) {
   	printf("Failed to allocate buffer struct.\n");
	return;
}

buffer->pageSize = 512;
buffer->numPages = M;
buffer->eraseSizeInPages = 8;
buffer->status = (id_t*) malloc(sizeof(id_t)*M);
if (buffer->status == NULL) {
	printf("Failed to allocate buffer status array.\n");
	return;
}

buffer->buffer  = malloc((size_t) buffer->numPages * buffer->pageSize);   
if (buffer->buffer == NULL) {
	printf("Failed to allocate buffer.\n");
	return;
}
buffer->blockBuffer = malloc((size_t) buffer->eraseSizeInPages * buffer->pageSize);
if (buffer->blockBuffer == NULL) {
	printf("Failed to allocate block buffer.\n");
	return;
}
buffer->storage = (storageState*) storage;

/* Configure Btree state */
vmtreeState* state = (vmtreeState*) malloc(sizeof(vmtreeState));
if (state == NULL) {
	printf("Failed to allocate VMtree state struct.\n");
	return;
}

state->recordSize = 16;			/* TODO: Set to your record size */
state->keySize = 4;				/* TODO: Set to your key size */
state->dataSize = 12;			/* TODO: Set to your data size */
state->buffer = buffer;       
state->tempKey = malloc(state->keySize); 
state->tempKey2 = malloc(state->keySize); 
int16_t ds = state->dataSize;
if (ds < state->keySize)
	ds = state->keySize;
state->tempData = malloc(ds);           	               
state->parameters = type;  
state->mappingBuffer = NULL;
state->mappingBufferSize = 0;

/* Initialize mapping table if using VMTREE variant */    
if (state->parameters == VMTREE) {   
	state->mappingBufferSize = 1024;
	state->mappingBuffer = malloc(state->mappingBufferSize);	
	if (state->mappingBuffer == NULL) {
		printf("Failed to allocate mapping buffer size: %d\n", state->mappingBufferSize);
		return;
	}
}

/* OPTIONAL: Enable log buffer by allocating space or set to NULL for no log buffer */
/* Log buffer enables higher insert performance by batching inserts. */
state->logBuffer = NULL;
state->logBufferSize = logBufferPages * buffer->pageSize;
if (state->logBufferSize > 0)
	state->logBuffer  = malloc(state->logBufferSize);  

/* Connections between buffer and VMTree */
buffer->activePath = state->activePath;
buffer->state = state;
buffer->isValid = vmtreeIsValid;
buffer->movePage = vmtreeMovePage;

/* Initialize VMTree structure with parameters */
vmtreeInit(state);
state->compareKey = compareKey;  /* Define function to compare keys */
```

### Insert (put) items into tree

```c
void *key, *data;
int8_t result = vmtreePut(state, key, data);
```

### Query (get) items from tree

```c
/* key points to key to search for. data must point to pre-allocated space to copy data into. */
int32_t key = 15;
void* data = malloc(state->dataSize);       
int8_t result = btreeGet(state, (void*) key, (void*) data);
```

### Iterate through items in tree

```c
vmtreeIterator it;
uint32_t minVal = 40;     /* Starting minimum value to start iterator (inclusive) */
it.minKey = &minVal;
uint32_t maxVal = 299;	  /* Maximum value to end iterator at (inclusive) */
it.maxKey = &maxVal;       

vmtreeInitIterator(state, &it);

uint32_t *itKey, *itData;	/* Pointer to key and data value. Valid until next call to btreeNext(). */

while (vmtreeNext(state, &it, (void**) &itKey, (void**) &itData))
{                      
	printf("Key: %lu  Data: %lu\n", *itKey, *itData);	
}
```


#### Ramon Lawrence<br>University of British Columbia Okanagan

