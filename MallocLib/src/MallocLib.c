#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <sys/mman.h>

#define HEADER_SIZE sizeof (struct memBlock)
#define ALIGNMENT 8 // must be a power of 2
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#ifndef STRATEGY
#define STRATEGY 4
#endif

#ifndef NRQUICKLISTS
#define NRQUICKLISTS 4
#endif


typedef struct memBlock {
	int free;
	size_t size;
    struct memBlock* next;
    struct memBlock* prev;
} memBlock;


static memBlock *listStart=NULL; // Points to the first block of the list
static memBlock *listStop=NULL; // Points to the last block of the list
static int maxBlockSizeQuickFit=0;

memBlock* extendHeap(size_t dataSize) {
	memBlock *newBlock;

//	newBlock = (memBlock *) mmap(0, dataSize+HEADER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	newBlock = sbrk(dataSize+HEADER_SIZE);
	if (newBlock==(void *) -1) {
		// mmap fail
		perror("mmap fail\n");
		return NULL;
	}

	newBlock->free = 0;
	newBlock->next = NULL;
	newBlock->prev = NULL;
	newBlock->size = dataSize;

	if (STRATEGY==4) {return newBlock;}	// Use quicklists, dont link here

	if(!listStop) {newBlock->prev=NULL;} // First call
	else {
		newBlock->prev=listStop;
		listStop->next=newBlock;
	}

	listStop=newBlock;
	return newBlock;
}

memBlock* bestFit(size_t dataSize) {
	memBlock *currentBlock, *returnBlock=NULL;
	currentBlock = listStart;

	int smallestBlockSize=-1;
	while(1) {
		if(currentBlock->free && currentBlock->size >= dataSize && (smallestBlockSize==-1 || smallestBlockSize>currentBlock->size)) {
			smallestBlockSize=currentBlock->size;
			returnBlock=currentBlock;
		}
		if (currentBlock->next==NULL) {break;}
		currentBlock=currentBlock->next;
	}

	if (returnBlock!=0) {returnBlock->free=0;}
	return returnBlock;
}

memBlock* worstFit(size_t dataSize) {
	memBlock *currentBlock, *returnBlock=NULL;
	currentBlock = listStart;

	int biggestBlockSize=-1;
	while(1) {
		if(currentBlock->free && currentBlock->size >= dataSize && (biggestBlockSize==-1 || biggestBlockSize<currentBlock->size)) {
			biggestBlockSize=currentBlock->size;
			returnBlock=currentBlock;
		}
		if (currentBlock->next==NULL) {break;}
		currentBlock=currentBlock->next;
	}

	if (returnBlock!=0) {returnBlock->free=0;}
	return returnBlock;
}

memBlock* firstFit(size_t dataSize) {
	memBlock *currentBlock;
	currentBlock = listStart;
	while(1) {
		if(currentBlock->free && currentBlock->size >= dataSize) {
			currentBlock->free=0;
			return currentBlock;
		}
		if (currentBlock->next==NULL) {return NULL;}
		currentBlock=currentBlock->next;
	}
}

int areBlocksContiguous (memBlock *leftBlock, memBlock *rightBlock) {
	// return 1 if left block is contiguous with right block. If false, there is an unmapped partial page in between
	if (rightBlock == (memBlock *) ( (void *)leftBlock + HEADER_SIZE + leftBlock->size)) {return 1;}
	else {return 0;}
}

void free(void * vPoint) {
	if (vPoint==0) {return;}
	memBlock *freeBlock = (memBlock *) (vPoint - HEADER_SIZE);
	freeBlock->free=1;

	if (STRATEGY==4) {return;}

	if (freeBlock->next && freeBlock->next->free && areBlocksContiguous(freeBlock,freeBlock->next)) {
		// Next block is also free
		freeBlock->size += (HEADER_SIZE + freeBlock->next->size);

		if (freeBlock->next->next) {
			freeBlock->next->next->prev = freeBlock;
			freeBlock->next = freeBlock->next->next;
		}
		else {
			listStop=freeBlock;
			freeBlock->next=NULL;
		}
	}
	if(freeBlock->prev && freeBlock->prev->free && areBlocksContiguous(freeBlock->prev,freeBlock)) {
		// Prev block is also free
		freeBlock = freeBlock->prev;
		freeBlock->size += (HEADER_SIZE + freeBlock->next->size);

		if (freeBlock->next->next) {
			freeBlock->next->next->prev = freeBlock;
			freeBlock->next = freeBlock->next->next;
		}
		else {
			listStop=freeBlock;
			freeBlock->next=NULL;
		}
	}

}

void splitBlock(memBlock * splitBlock, size_t dataSize) {
	// Split the block
	memBlock* newBlock = (memBlock *) ( (void*)splitBlock + HEADER_SIZE + dataSize );

	newBlock->free=1;

	newBlock->size=splitBlock->size - dataSize - HEADER_SIZE;
	splitBlock->size = dataSize;

	// Link the new block
	if (splitBlock->next) {splitBlock->next->prev=newBlock;}
	else {listStop = newBlock;}
	newBlock->next=splitBlock->next;
	splitBlock->next = newBlock;
	newBlock->prev = splitBlock;

	free((void *) newBlock + HEADER_SIZE);
}

void *malloc(size_t dataSize) {
	if (dataSize==0) {return (void*) 0;}
	memBlock *reqBlock;

	if (STRATEGY==4 && !listStart) {
		// Lists are not initalized, initalize
		maxBlockSizeQuickFit = 8*pow(2,NRQUICKLISTS);

		reqBlock = extendHeap(0);
		listStart = reqBlock;

		int i;
		for (i=1;i<NRQUICKLISTS;i++) {reqBlock = extendHeap(0);} // Add first entries to all lists
	}

	if (STRATEGY==4 && dataSize <=maxBlockSizeQuickFit) {
		// Which list is apropriate for datasize?
		int po2=8,i=0;
		while(1) {
			if (po2>=dataSize) {break;}
			i++;
			po2 = po2*2;
		}
		// Data will be stored on list i
		if (i>=NRQUICKLISTS) {return NULL;}
		reqBlock = (memBlock*) ( (void*)listStart + i*HEADER_SIZE);

		while(1) {
			if (reqBlock->free) {
				reqBlock->free=0;
				return (void *)reqBlock + HEADER_SIZE;
			}

			if (reqBlock->next==NULL) {
				reqBlock->next = extendHeap(po2);
				return (void *)reqBlock->next + HEADER_SIZE;
			}
			reqBlock=reqBlock->next;
		}
	}

	// Use first/best/worst fit
	dataSize = ALIGN(dataSize+HEADER_SIZE);
	dataSize -= HEADER_SIZE;

	if (!listStart) {
		// First call
		reqBlock = extendHeap(dataSize);
		if (!reqBlock) {return NULL;} //mmap fail

		listStart = reqBlock;

		return (void *)reqBlock + HEADER_SIZE;
	}

	if (STRATEGY==1 || STRATEGY==4) {reqBlock=firstFit(dataSize);}
	else if (STRATEGY==2) {reqBlock=bestFit(dataSize);}
	else if (STRATEGY==3) {reqBlock=worstFit(dataSize);}

	if (!reqBlock) {reqBlock = extendHeap(dataSize);} // No space in list, allocate
	if (!reqBlock) {return NULL;} // mmap fail
	if (reqBlock->size > dataSize+HEADER_SIZE) {splitBlock(reqBlock,dataSize);} // Split the block

	return (void *)reqBlock + HEADER_SIZE;
}

void *realloc(void* vPoint, size_t dataSize) {
	if (vPoint==0) {return malloc(dataSize);}
	if (dataSize==0) {return (void*) 0;}

	memBlock *oldBlock = (memBlock *) (vPoint - HEADER_SIZE);
	void *newBlock = malloc(dataSize);

	int limit;
	if (dataSize > oldBlock->size) {
		limit = oldBlock->size;
	}
	else {limit = dataSize;}

	int i;
	char *newpos = newBlock, *oldpos = vPoint;
	for (i=0;i<limit;i++) {
		*newpos=*oldpos;
		newpos++;
		oldpos++;
	}

	free(vPoint);
	return newBlock;
}




