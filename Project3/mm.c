/*
 * mm-segregated list
 * Allocated block has header and footer
 * Maintain lists of free blocks, for each size class
 * Free block has header, footer and Next/Prev free block pointer
 * Two additional MACRO: NEXT_FBLKP(bp), PREV_FBLKP(bp)
 * 
 * Placement policy: First fit and splitting
 * Coalescing policy: immediate
 * Free list insertion policy: LIFO
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20181685",
    /* Your full name*/
    "Jaewoong Jung",
    /* Your email address */
    "jaewoong_jung@naver.com",
};

/* single word (4) or double word (8) alignment */
#define WSIZE           4
#define DSIZE           8
#define CHUNKSIZE       (1<<8)
#define CLASSCNT        12
#define ALLOC_BIT       0x1
// #define PALLOC_BIT      0x2

#define MAX(x, y)       ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))
#define PUT_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

/* Read the size and allocated fields from address p: p is a header or footer */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & ALLOC_BIT)
// #define GET_PALLOC(p)   (GET(p) & PALLOC_BIT)

/* Given block ptr bp, compute address of its header and footer: bp points starting point of payload */
#define HDRP(bp)        ((char*)(bp) - WSIZE)
#define FTRP(bp)        ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

/* Given block ptr bp, compute address of next and previous FREE blocks */
#define NEXT(bp)        ((char*)(bp))
#define PREV(bp)        ((char*)(bp) + WSIZE)
#define NEXT_FBLKP(bp)  (*(void**)(bp))
#define PREV_FBLKP(bp)  (*(void**)((bp) + WSIZE))

#define ALIGNMENT 8
#define MIN_BLK_SIZE (2*DSIZE)

static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void cut(void *bp, size_t size);

void insert_node(void *list, void *node);
void delete_node(void *node);
void *get_free_list(int class_cnt);
int get_size_class(size_t size);
void mm_check(void);

void *heap_listp;
void *free_list_start;


/* 
 * mm_init - initialize the malloc package.
 * mem_init initializes out heap but the brk pointer remains at starting point.
 * we should extend out heap by using mem_sbrk
 */
int mm_init(void) {
    void *free_listp;
    /*  header, next, prev, footer: 4 words for each free_list header 
     *  padding(1), prolog(2), epilog(1): 4 additional words
     */
    size_t init_size = (4*CLASSCNT) + 4;            
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(init_size*WSIZE)) == (void*)-1)
        return -1;
    PUT(heap_listp, 0);                                 /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));      /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));      /* Prologue footer */
    free_list_start=heap_listp+(4*WSIZE);
    free_listp=free_list_start;
    for(int i=0; i<CLASSCNT; i++){
        PUT(HDRP(free_listp), PACK(2*DSIZE, 1));
        PUT_PTR(free_listp, free_listp + WSIZE);            /* Circular doubly linked list */
        PUT_PTR(free_listp + WSIZE, free_listp);
        PUT(FTRP(free_listp), PACK(2*DSIZE, 1));
        free_listp += (2*DSIZE);
    }
    PUT(heap_listp + ((init_size-1)*WSIZE), PACK(0, 1));            /* Epilogue header */

    heap_listp += (2*WSIZE);                        // jump prologue header
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1;
    return 0;
}

/* extend_heap - extends the heap by CHUNKSIZE bytes and 
 * creates the initial free block. 
 * invoked in 2 cases: 
 *      1. heap is initialized in mm_init
 *      2. mm_malloc cannot find a suitable fit.
 */
static void* extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words%2) ? (words+1)*WSIZE : words*WSIZE;

    if((long)(bp = mem_sbrk(size)) == -1)       //  mem_sbrk will return old_brk
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));               /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));               /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));       /* New epiilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* get_free_list - has one input size_class
 * return appropriate free list
 */
void *get_free_list(int size_class){
    void *free_listp = free_list_start;
    for(int i=0; i<size_class; i++)
        free_listp += (2*DSIZE);
    return free_listp;
}

/* get_size_class - has one input size
 * return paaroprate free list
 * size_class is separated by the power of 2
 * The last size class will contain remaining other large numbers
 */
int get_size_class(size_t size){
    int size_class=0;
    size >>= 4;
    while(size){
        /* power of 2 */
        size>>=1;
        size_class++;
    }
    if(size_class >= CLASSCNT) 
        /* last size class will contain other large numbers */
        size_class=CLASSCNT-1;

    return size_class;
}

/* Insert_node - Wrapper function of insert_node */
void Insert_node(size_t size, void *node){
    void *free_listp = get_free_list(get_size_class(size));
    insert_node(free_listp, node);
}

/* insert_node - insert node into list
 * list is seleted in the function Insert_node()
 * insertion policy: LIFO
 */
void insert_node(void *list, void *node) {
    PUT_PTR(PREV(node), PREV_FBLKP(list));
    PUT_PTR(NEXT(node), list);
    PUT_PTR(NEXT(PREV_FBLKP(list)), node);
    PUT_PTR(PREV(list), node);
}

/* delete_node - delete node
 * this function doesn't need list pointer
 * since all free lists are doubly linked list
 */
void delete_node(void *node) {
    PUT_PTR(PREV(NEXT_FBLKP(node)), PREV_FBLKP(node));
    PUT_PTR(NEXT(PREV_FBLKP(node)), NEXT_FBLKP(node));
}

/* coalesce - coalesce if adjacent blocks are free
 * invoked in 2 cases
 *      1. after mm_free
 *      2. after extend_heap
 * In both cases, it is assumed that bp is already freed.
 */
static void* coalesce(void* bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc){
        /* do nothing */
    }
    else if (prev_alloc && !next_alloc) {           /* case 2 */
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {           /* case 3 */
        delete_node(PREV_BLKP(bp));      
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp=PREV_BLKP(bp);
    }
    else {                                          /* case 4 */
        delete_node(NEXT_BLKP(bp));
        delete_node(PREV_BLKP(bp));
        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp=PREV_BLKP(bp);
    }
    Insert_node(GET_SIZE(HDRP(bp)), bp);
    return bp;
}

/* find_fit - first fit search
 * if no appropriate fit found, return NULL
 */
static void* find_fit(size_t asize){
    void *free_listp;   //  free list pointer for given size class
    void* bp;
    int size_class;
    
    size_class = get_size_class(asize);
    
    while(size_class < CLASSCNT){
        free_listp = get_free_list(size_class);

        for(bp=PREV_FBLKP(free_listp); !GET_ALLOC(HDRP(bp)); bp=PREV_FBLKP(bp))
            if(GET_SIZE(HDRP(bp))>=asize)
                return bp;
        size_class++;
    }
    /* no fit found */
    return NULL;
}

/* place - allocate block
 * if remainder is larger than a threshold, cut the block
 */
static void place(void *bp, size_t asize) {
    size_t remainder = GET_SIZE(HDRP(bp)) - asize;

    delete_node(bp);
    if(remainder >= MIN_BLK_SIZE){
        cut(bp, asize);
    }
    else{
        //  if remainder is less than minumum block size, allocate whole block
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    }

    
}

/* cut - store the remainder into free list
 * assumed that bp will be allocated as much as size
 * and the remainder is enough to be stored in some free list
 */
static void cut(void *bp, size_t size){
    size_t remainder = GET_SIZE(HDRP(bp)) - size;
    // allocated block
    PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));
    // splitted block
    PUT(HDRP(NEXT_BLKP(bp)), PACK(remainder, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(remainder, 0));
    Insert_node(remainder, NEXT_BLKP(bp));
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize;           /* Adjusted block size */
    size_t extendsize;      /* Amount to extend heap if no fit */
    char *bp;

    /*Ignore spurious requests */
    if (size <= 0) return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if(size <= DSIZE)           //  payload <= DSIZE
        asize = MIN_BLK_SIZE;   //  minimum block size = 3*DSIZE (header+footer=DSIZE, next/prev=DSIZE)
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    

    /* Search the free list for a fit */
    if ((bp=find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp=extend_heap(extendsize/WSIZE)) == NULL){
        fprintf(stderr, "extend heap error in mm_malloc \n");
        return NULL;
    }

    place(bp, asize);

    /* MM_CHECK */
//    mm_check();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);

    /* MM_CHECK */
//    mm_check();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size) {
    void *newptr;
    size_t asize, bp_size, remainder;
    if (bp == NULL) return malloc(size);
    if (size == 0){
        mm_free(bp);
        return NULL;
    }

    /* size alignment */
    bp_size = GET_SIZE(HDRP(bp));
    if(size <= DSIZE)           
        asize = MIN_BLK_SIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if(asize <= bp_size){
        /* if requested size is less than of equal to current bp size,
         * no need to additional mm_malloc call
         * just modify current bp and return it
         */
        remainder = bp_size - asize;
        if(remainder >= MIN_BLK_SIZE){
            /* if remainder is larger than some threshold, cut it */
            cut(bp, asize);
        }
        else{
            PUT(HDRP(bp), PACK(bp_size, 1));
            PUT(FTRP(bp), PACK(bp_size, 1));
        }
        newptr=bp;
    }
    else{   
        /* requested size > bp_size */
        size_t total_size;
        total_size = GET_SIZE(HDRP(NEXT_BLKP(bp))) + bp_size;
        if(!GET_ALLOC(HDRP(NEXT_BLKP(bp))) && total_size >= asize){
            /* if next block is free and total size is larger than requested size,
             * we can coalesce two blocks and return the pointer of the coalesced block
             * Enough to find just one successor block, since it is assumed that
             * every successive free block would be already coalesced.
             */
            delete_node(NEXT_BLKP(bp));
            PUT(HDRP(bp), PACK(total_size, 1));
            PUT(FTRP(bp), PACK(total_size, 1));
            remainder = total_size - asize;
            newptr=bp;
        }
        else{
            newptr=mm_malloc(asize);
            memcpy(newptr, bp, bp_size);
            mm_free(bp);
        }
    }
/* MM_CHECK */
//    mm_check();
    return newptr;
}

void mm_check(void){
    void *free_listp;
    /* Case 1: Every free list contains appropriate blocks? */
    for(int class=0; class<CLASSCNT; class++){
        void *bp;
        size_t size;
        free_listp = get_free_list(class);
        for(bp=PREV_FBLKP(free_listp); !GET_ALLOC(HDRP(bp)); bp=PREV_FBLKP(bp)){
            size = GET_SIZE(HDRP(bp));
            if(get_size_class(size) != class)
                fprintf(stderr, "mm_check CASE 1 ERROR \n");
        }
    }

    /* Case 2: Every free block does not have allocation bit? */
    for(int class=0; class<CLASSCNT; class++){
        void *bp;
        int alloc;
        free_listp = get_free_list(class);
        for(bp=PREV_FBLKP(free_listp); !GET_ALLOC(HDRP(bp)); bp=PREV_FBLKP(bp)){
            alloc = GET_ALLOC(HDRP(bp));
            if(alloc == ALLOC_BIT)
                fprintf(stderr, "mm_check CASE 2 ERROR \n");
        }
    }
    
}
