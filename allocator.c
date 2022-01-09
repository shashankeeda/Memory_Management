/**
 * @file
 *
 * Explores memory management at the C runtime level.
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>

#include "allocator.h"
#include "logger.h"

static struct mem_block *g_head = NULL; /*!< Start (head) of our linked list */
static struct mem_block *g_tail = NULL;

static int page_size=0;
static unsigned long g_allocations = 0; /*!< Allocation counter */
static unsigned long g_regions = 0; /*!< Allocation counter */

pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER; /*< Mutex for protecting the linked list */

/**
 * Given a free block, this function will split it into two pieces and update
 * the linked list.
 *
 * @param block the block to split
 * @param size new size of the first block after the split is complete,
 * including header sizes. The size of the second block will be the original
 * block's size minus this parameter.
 *
 * @return address of the resulting second block (the original address will be
 * unchanged) or NULL if the block cannot be split.
 */
struct mem_block *split_block(struct mem_block *block, size_t size)
{
    // TODO block splitting algorithm
    if(block==NULL){
	return NULL;
    }
    int x=block->size - size+sizeof(struct mem_block);
    int y=(sizeof(struct mem_block)+8);
    if(x<y){
	return NULL;
    }
    bool truth=false;
    if(block->next==NULL){
	page_size=page_size-size+100;
	if(page_size<108){
		page_size=page_size+size-100;
		return NULL;
	}
	truth=true;
    }
    void *temp=(void *)block+size;
    struct mem_block *temp2=(struct mem_block *)temp;
    if(!truth){
	temp2->size=block->size-size;	
	temp2->free=true;
	temp2->region_id=block->region_id;
	temp2->next=block->next;
	block->next->prev=temp2;
	block->size=size-sizeof(struct mem_block);
	block->next=temp2;
	temp2->prev=block;
    }
    else{
	page_size=page_size-100;
	temp2->size=page_size;
	temp2->free=true;
	block->next=temp2;
	block->size=size-sizeof(struct mem_block);
	temp2->prev=block;
	temp2->next=NULL;
	temp2->region_id=block->region_id;
	g_tail=temp2;
    }
    
    return temp;
}

/**
 * Given a free block, this function attempts to merge it with neighboring
 * blocks --- both the previous and next neighbors --- and update the linked
 * list accordingly.
 *
 * @param block the block to merge
 *
 * @return address of the merged block or NULL if the block cannot be merged.
 */
struct mem_block *merge_block(struct mem_block *block)
{
    // TODO block merging algorithm
    if(block==NULL){
	return NULL;
    }
    
    if(block->next!=NULL && block->next->free==true && block->next->region_id==block->region_id){
	struct mem_block *right=block->next;
	block->next=right->next;
	if(right->next!=NULL){
		right->next->prev=block;
	}
	block->size=block->size+right->size+100;
    }
    if(block->prev!=NULL && block->prev->free==true && block->prev->region_id==block->region_id){
	struct mem_block *left=block->prev;
	left->next=block->next;
	if(block->next!=NULL){
		block->next->prev=left;
	}
	left->size=left->size+block->size+100;
	return left;
    }
    
    return block;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * first fit free space management algorithm.
 *
 * @param size size of the block (header + data)
 */
void *first_fit(size_t size)
{
    // TODO: first fit FSM implementation
    struct mem_block *temp=g_head;
    while(temp!=NULL){
	if(temp->size>=size&&temp->free==true){
		return temp;
	}
	temp=temp->next;
    }
    return NULL;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * worst fit free space management algorithm. If there are ties (i.e., you find
 * multiple worst fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *worst_fit(size_t size)
{
    // TODO: worst fit FSM implementation
    struct mem_block *temp=g_head;
    struct mem_block *block=NULL;
    int len=0;
    while(temp!=NULL){
	if(temp->size>=size && temp->size>len && temp->free==true){
		len=temp->size;
		block=temp;
	}
	temp=temp->next;
    }
    return block;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * best fit free space management algorithm. If there are ties (i.e., you find
 * multiple best fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *best_fit(size_t size)
{
    // TODO: best fit FSM implementation
    struct mem_block *temp=g_head;
    int len=INT_MAX;
    struct mem_block *block=NULL;
    while(temp!=NULL){
	if(temp->size>=size && temp->size<len && temp->free==true){
		len=temp->size;
		block=temp;
	}
	temp=temp->next;
    }
    return block;
}

/**
 *
 *Allows us to check if there is any block we can reuse
 *@param size is the size of the block we are checking to see if we can reuse
 *
 */
void *reuse(size_t size)
{
    // TODO: using free space management (FSM) algorithms, find a block of
    // memory that we can reuse. Return NULL if no suitable block is found.
    if(size<=0){
	return NULL;
    }
    struct mem_block *temp=g_head;
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
   	 algo = "first_fit";
    }
    if(strcmp(algo,"first_fit")==0){
	temp=first_fit(size);
	return temp;
    }
    else if(strcmp(algo,"worst_fit")==0){
	temp=worst_fit(size);
	return temp;
    }
    if(strcmp(algo,"best_fit")==0){
	temp=best_fit(size);
	return temp;
    }
    return NULL;
}

/**
 *
 *Allocates a memory block of the requested size either by reusing or mmaping
 *@param size is the amount of bytes the user wants
 *
 *
 */
void *malloc(size_t size)
{
    pthread_mutex_lock(&alloc_mutex);
    char *scribble=getenv("ALLOCATOR_SCRIBBLE");
    int s=0;
    int y=size+sizeof(struct mem_block);
    if(y%8!=0){
	size=(size+sizeof(struct mem_block)+(8-((size+sizeof(struct mem_block))%8)))-sizeof(struct mem_block);
    }
    while(s<y){
	s=s+getpagesize();
    }
    size_t mem_size = s;
    int prot_flags = PROT_READ | PROT_WRITE;
    int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;

    struct mem_block *block=reuse(size);
    if(block==NULL){
    	block = mmap(NULL, mem_size, prot_flags, map_flags, -1, 0);
    
    	if(block==MAP_FAILED){
		perror("mmap");
    		pthread_mutex_unlock(&alloc_mutex);
		return NULL;
	    }
	page_size=mem_size;
    	if(g_head==NULL && g_tail == NULL){
		g_head=block;
		g_tail=g_head;
		block->prev=NULL;
	    }
	    else{
		g_tail->next=block;
		block->prev=g_tail;
		g_tail=block;
	    }
	page_size=page_size-100;
    	block->region_id = ++g_regions;
    	block->next=NULL;
	block->size=page_size;
    }
    g_allocations++;
    block->free=false;

    split_block(block,size+sizeof(struct mem_block));

    if(scribble && strcmp(scribble,"1")==0){
	memset(block+1,0xAA,block->size);
    }
    pthread_mutex_unlock(&alloc_mutex);
    return block+1;
}

/**
 *
 *Basically calls malloc and sets the name
 *@param size is the amount of bytes we want
 *@param name is the name we want to se it too
 *
 */
void *malloc_name(size_t size, char *name)
{
	void *ptr=malloc(size);
    	pthread_mutex_lock(&alloc_mutex);
	if(ptr!=NULL){
		struct mem_block *block = (struct mem_block *) ptr-1;
		strncpy(block->name,name,32);
	}
    	pthread_mutex_unlock(&alloc_mutex);
	return ptr;
	
}

/**
 *
 *Frees up memory for other uses
 *@param ptr is the memory block we are trying to free
 *
 */
void free(void *ptr)
{
    pthread_mutex_lock(&alloc_mutex);
    if (ptr == NULL) {
        /* Freeing a NULL pointer does nothing */
    	pthread_mutex_unlock(&alloc_mutex);
        return;
    }

    // TODO: free memory. If the containing region is empty (i.e., there are no
    // more blocks in use), then it should be unmapped.
    
    struct mem_block *block=(struct mem_block *) ptr-1;
    if(block!=NULL){
	block->free=true;
    }
    
    /*
    block=merge_block(block);
    if(block && (block->prev==NULL||block->prev->region_id!=block->region_id) && (block->next==NULL||block->next->region_id!=block->region_id)){
	if(block->next!=NULL){
		block->next->prev=block->prev;
	}
	if(block->prev!=NULL){
		block->prev->next=block->next;
	}
	block->next=NULL;
	block->prev=NULL;
	munmap(block,block->size+100);
    }
    */
    pthread_mutex_unlock(&alloc_mutex);
}

/**
 *
 *Basically calls malloc giving the amount of members and the size of the members
 *@param nmemb is the number of members
 *@param size is the size of each member
 *
 *
 */
void *calloc(size_t nmemb, size_t size)
{
    // TODO: hmm, what does calloc do?

    void *block = malloc(nmemb*size);
    pthread_mutex_lock(&alloc_mutex);
    memset(block,0,nmemb*size);
    pthread_mutex_unlock(&alloc_mutex);
    return block;
}

/**
 *
 *Sees if we can reuse any block, otherwises mallocs memory and copies over the old memory
 *@param ptr is the memory block we are trying to realloc
 *@param size is the size we want to realloc
 *
 *
 */
void *realloc(void *ptr, size_t size)
{
    pthread_mutex_lock(&alloc_mutex);
    if (ptr == NULL) {
        /* If the pointer is NULL, then we simply malloc a new block */
    	pthread_mutex_unlock(&alloc_mutex);
        return malloc(size);
    }

    if (size == 0) {
        /* Realloc to 0 is often the same as freeing the memory block... But the
         * C standard doesn't require this. We will free the block and return
         * NULL here. */
    	pthread_mutex_unlock(&alloc_mutex);
        free(ptr);
        return NULL;
    }
    struct mem_block *block=(struct mem_block *) ptr-1;
    if(block->size>=size){
    	pthread_mutex_unlock(&alloc_mutex);
	return ptr;
    }

    pthread_mutex_unlock(&alloc_mutex);
    void *temp=malloc(size);
    pthread_mutex_lock(&alloc_mutex);
    if(temp!=NULL){
    	memcpy(temp,ptr,block->size);
    	block->free=true;
    	pthread_mutex_unlock(&alloc_mutex);
    	return temp;
    }
    pthread_mutex_unlock(&alloc_mutex);

    return NULL;
}

/**
 * print_memory
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 */
void print_memory(void)
{
    puts("-- Current Memory State --");
    struct mem_block *current_block = g_head;
    //struct mem_block *current_region = NULL;
    // TODO implement memory printout
    unsigned long current_region=0;
    int x=0;

    while(current_block!=NULL){
		if((current_block->region_id!=current_region || current_block==g_head)&&x!=current_block->region_id){
			printf("[REGION %lu] %p\n",current_block->region_id,current_block);
			x++;
		}

		printf("  [BLOCK] %p-%p '%s' %zu [%s]\n", 
				current_block, 
				(char *) current_block + current_block->size, 
				current_block->name,
				current_block->size,
				current_block->free ? "FREE" : "USED");
		current_block=current_block->next;
    }
}
