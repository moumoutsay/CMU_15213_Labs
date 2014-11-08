/*
 * mm.c
 *
 * @author Wei-Lin Tsai weilints@andrew.cmu.edu
 * 
 * Approach: segregated fits decribed on p. 871 
 * Search method: first fit from minimum sized free list 
 * Coalescing policy: immediate
 *
 * Howto:
 *   == init ==
 *      1. reset all free-lists
 *      2. create a first block of heap as 
 *         Fig. 9.42 on page 863
 *      3. use extend_heap function to allocate a basic size of heap
 *
 *   == malloc ==
 *      1. Find a suitable block from free list (use first-fit 
 *         search in each list)
 *      2. If found 
 *          2.a allocate memory(set allocate bit), do split if acceptable
 *          2.b remove block from list 
 *          2.c inserted a splited block into list(if any)
 *      3. If not found 
 *          3.a extend heap 
 *          3.b format extended space as a freed block
 *          3.c allocate memory(set allocate bit), do split if acceptable
 *          3.d remove block from list 
 *          3.e inserted a splited block into list(if any)
 *
 *   == free ==
 *      1. Free block by resetting allocat bit
 *      2. Do coalesce if acceptable
 *      3. Insert to target list by its size
 *
 * --
 *
 * Data structure of block:
 *   Same as Fig 9.48(a)(b) on textbook p. 869
 *   Allocated:
 *      31-------------0-
 *      | block size|a/f|  Header
 *      -----------------
 *      | payload       |
 *      -----------------
 *      | padding(opt)  |
 *      -----------------
 *      | block size|a/f|  Footer 
 *      -----------------
 *   Freed
 *      31-------------0-
 *      | block size|a/f|  Header
 *      -----------------
 *      | pred ptr      |  Pointer to predecessor
 *      -----------------
 *      | succ ptr      |  Pointer to successor
 *      -----------------
 *      | padding(opt)  |
 *      -----------------
 *      | block size|a/f|  Footer 
 *      -----------------
 *   As a result 
 *   Header/Footer = 4/4; ptr to pred/succ = 8/8. total is 24 byes 
 *
 * -- 
 *
 * Free lists organization 
 *   We have 12 free-lists, denoted as free_list_size_1_head 
 *   ~ free_list_size_2048_head
 *   the size of each list is defined ad TYPE_1_SIZE ~ TYPE_2048_SIZE
 *
 * --
 *
 * @note
 *      Follow the most basic approach from text book.
 * @attention 
 *      Use contracts to check behavior in debug mode.
 *      Hence, not many error condition handeling in normal mode
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "contracts.h"

#include "mm.h"
#include "memlib.h"


// Create aliases for driver tests
// DO NOT CHANGE THE FOLLOWING!
#ifdef DRIVER
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif

/*
 *  Logging Functions
 *  -----------------
 *  - dbg_printf acts like printf, but will not be run in a release build.
 *  - checkheap acts like mm_checkheap, but prints the line it failed on and
 *    exits if it fails.
 */

#ifndef NDEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#define checkheap(verbose) do {if (mm_checkheap(verbose)) {  \
                             printf("Checkheap failed on line %d\n", __LINE__);\
                             exit(-1);  \
                        }}while(0)
#else
#define dbg_printf(...)
#define checkheap(...)
#endif

/** Constant/Magic number definition */
#define ALIGNMENT   8       /// 8 bytes alignment by lab's requirement
#define WSIZE       4       /// Word and header/footer size (bytes) 
#define DSIZE       8       /// Doubleword size (bytes)
#define CHUNKSIZE  (1<<7)   /// Extend heap by this amount (bytes)

/** header/footer = 4/4; ptr to pred/succ = 8/8. total is 24 byes */
#define MIN_BLK_SIZE    24   /// minimum block size for double link-list block
/** minimum size of each free-list */
#define TYPE_1_SIZE     24 * 1
#define TYPE_2_SIZE     24 * 2
#define TYPE_4_SIZE     24 * 4
#define TYPE_8_SIZE     24 * 8 
#define TYPE_16_SIZE    24 * 16
#define TYPE_32_SIZE    24 * 32
#define TYPE_64_SIZE    24 * 64
#define TYPE_128_SIZE   24 * 128
#define TYPE_256_SIZE   24 * 256
#define TYPE_512_SIZE   24 * 512
#define TYPE_1024_SIZE  24 * 1024
#define TYPE_2048_SIZE  24 * 2048
#define NO_OF_SIZE_TYPE 12   /// number of different type size

/** Global variables */
static char *heap_listp = 0;     /// Pointer to first block
/** use 12 global ptr point to each sized free list */
static char *free_list_size_1_head   = 0; /// list index = 0  
static char *free_list_size_2_head   = 0; /// list index = 1
static char *free_list_size_4_head   = 0; /// list index = 2
static char *free_list_size_8_head   = 0; /// list index = 3
static char *free_list_size_16_head  = 0; /// list index = 4
static char *free_list_size_32_head  = 0; /// list index = 5
static char *free_list_size_64_head  = 0; /// list index = 6
static char *free_list_size_128_head = 0; /// list index = 7
static char *free_list_size_256_head = 0; /// list index = 8
static char *free_list_size_512_head = 0; /// list index = 9
static char *free_list_size_1024_head= 0; /// list index =10
static char *free_list_size_2048_head= 0; /// list index =11

/*
 *  Helper functions
 *  ----------------
 */
// Align p to a multiple of w bytes
static inline void* align(const void const* p, unsigned char w) {
    return (void*)(((uintptr_t)(p) + (w-1)) & ~(w-1));
}

// Check if the given pointer is 8-byte aligned
static inline int aligned(const void const* p) {
    return align(p, ALIGNMENT) == p;
}

// Return whether the pointer is in the heap.
static int in_heap(const void* p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/** Wei-Lin's helper functions begin */

/** Read and write a word at address p */
static inline unsigned int get_word_val(unsigned int *p) {
    REQUIRES(in_heap(p));
    return *p;
}
static inline void set_word_val(unsigned int *p, unsigned int val) {
    REQUIRES(in_heap(p));
    *p = val;
}

/** Pack a size and allocated bit into a word */
static inline unsigned int pack_size(unsigned int size, unsigned int alloc) {
    return ( size | alloc );
}

/** Read the size and allocated fields from address p */
static inline unsigned int get_size(unsigned int *p){
    REQUIRES(in_heap(p));
    return ( get_word_val(p) & ~0x7);
}
static inline unsigned int get_alloc(unsigned int *p){
    REQUIRES(in_heap(p));
    return ( get_word_val(p) & 0x1);
}

/** Given block ptr bp, compute address of its header and footer */
static inline unsigned int *get_header_ptr(char *bp){
    REQUIRES(NULL != bp);
    REQUIRES(aligned(bp));
    return (unsigned int *)(bp - WSIZE);
}
static inline unsigned int *get_footer_ptr(char *bp){
    REQUIRES(NULL != bp);
    REQUIRES(in_heap(bp));
    REQUIRES(aligned(bp));
    return (unsigned int *) \
        (bp + get_size(get_header_ptr(bp)) - DSIZE);
}

/** Given block ptr bp, compute address of next and previous blocks */
static inline char *get_next_blk_ptr(char *bp) {
    REQUIRES(NULL != bp);
    REQUIRES(in_heap(bp));
    REQUIRES(aligned(bp));
    return (bp + get_size((unsigned int *)(bp - WSIZE))); 
}
static inline char *get_prev_blk_ptr(char *bp) {
    REQUIRES(NULL != bp);
    REQUIRES(in_heap(bp));
    REQUIRES(aligned(bp));
    return (bp - get_size((unsigned int *)(bp - DSIZE))); 
}

/** Set pred/succ pointer of given block */
static inline void set_pred_ptr(void *bp, void *pred_ptr){
    REQUIRES(NULL != bp);
    REQUIRES(in_heap(bp));
    REQUIRES(aligned(bp));
    REQUIRES(aligned(pred_ptr));
    *(size_t*)bp = (uintptr_t)pred_ptr;
}
static inline void set_succ_ptr(void *bp, void *succ_ptr){
    REQUIRES(NULL != bp);
    REQUIRES(in_heap(bp));
    REQUIRES(aligned(bp));
    REQUIRES(aligned(succ_ptr));
    *((size_t*)bp + 1) = (uintptr_t)succ_ptr;
}

/** Reset block's pointers to NULL */ 
static inline void reset_list_note (void *bp){
    REQUIRES(NULL != bp);
    REQUIRES(in_heap(bp));
    REQUIRES(aligned(bp));
    // reset to be null
    set_pred_ptr(bp, NULL);
    set_succ_ptr(bp, NULL);
}

/** get pred/succ pointer of given block */
static inline char *get_pred_ptr(void *bp){
    REQUIRES(NULL != bp);
    REQUIRES(in_heap(bp));
    REQUIRES(aligned(bp));
    return (char *)(*(long*)bp);
}
static inline char *get_succ_ptr(void *bp){
    REQUIRES(NULL != bp);
    REQUIRES(in_heap(bp));
    REQUIRES(aligned(bp));
    return (char *)(*((long*)bp + 1));
}

/** given list_index, return corresponding free-list head*/
static inline char *get_list_head_by_index(size_t list_index){
    switch (list_index) {
        case 0:
            return free_list_size_1_head;
        case 1:
            return free_list_size_2_head;
        case 2:
            return free_list_size_4_head;
        case 3:
            return free_list_size_8_head;
        case 4:
            return free_list_size_16_head;
        case 5:
            return free_list_size_32_head;
        case 6:
            return free_list_size_64_head;
        case 7:
            return free_list_size_128_head;
        case 8:
            return free_list_size_256_head;
        case 9:
            return free_list_size_512_head;
        case 10:
            return free_list_size_1024_head;
        case 11:
            return free_list_size_2048_head;
        default:
            return free_list_size_1_head;
    }
}

/** given blk's size, return the list_index which it should fall*/
static inline size_t get_list_index_by_size(size_t size){
    if ( size <= TYPE_1_SIZE) {
        return 0;  
    } else if (size <= TYPE_2_SIZE) {
        return 1;
    } else if (size <= TYPE_4_SIZE) {
        return 2;
    } else if (size <= TYPE_8_SIZE) {
        return 3;
    } else if (size <= TYPE_16_SIZE) {
        return 4;
    } else if (size <= TYPE_32_SIZE) {
        return 5;
    } else if (size <= TYPE_64_SIZE) {
        return 6;
    } else if (size <= TYPE_128_SIZE) {
        return 7;
    } else if (size <= TYPE_256_SIZE) {
        return 8;
    } else if (size <= TYPE_512_SIZE) {
        return 9;
    } else if (size <= TYPE_1024_SIZE) {
        return 10;
    } else {
        return NO_OF_SIZE_TYPE - 1;
    }
}

/** given blk's size, return address(pointer) of corresponding
 *  list_head ptr */
static inline char **get_list_head_by_size(size_t size){
    if ( size <= TYPE_1_SIZE) {
        return &free_list_size_1_head;  
    } else if (size <= TYPE_2_SIZE) {
        return &free_list_size_2_head;
    } else if (size <= TYPE_4_SIZE) {
        return &free_list_size_4_head;
    } else if (size <= TYPE_8_SIZE) {
        return &free_list_size_8_head;
    } else if (size <= TYPE_16_SIZE) {
        return &free_list_size_16_head;
    } else if (size <= TYPE_32_SIZE) {
        return &free_list_size_32_head;
    } else if (size <= TYPE_64_SIZE) {
        return &free_list_size_64_head;
    } else if (size <= TYPE_128_SIZE) {
        return &free_list_size_128_head;
    } else if (size <= TYPE_256_SIZE) {
        return &free_list_size_256_head;
    } else if (size <= TYPE_512_SIZE) {
        return &free_list_size_512_head;
    } else if (size <= TYPE_1024_SIZE) {
        return &free_list_size_1024_head;
    } else {
        return &free_list_size_2048_head;
    }
}

/** Reset all linked-list */
static inline void reset_all_list_head(){
    free_list_size_1_head   = NULL;
    free_list_size_2_head   = NULL;
    free_list_size_4_head   = NULL;
    free_list_size_8_head   = NULL;
    free_list_size_16_head  = NULL;
    free_list_size_32_head  = NULL;
    free_list_size_64_head  = NULL;
    free_list_size_128_head = NULL;
    free_list_size_256_head = NULL;
    free_list_size_512_head = NULL;
    free_list_size_1024_head = NULL;
    free_list_size_2048_head = NULL;
}

/// Wei-Lin's helper functions end



/** Functions prototype declarations */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *find_fit_from_list(size_t asize, size_t list_index);
static void place(void *bp, size_t asize);
static void list_push_front(char *bp);
static void remove_from_list(char *bp);

/*
 *  Malloc Implementation
 *  ---------------------
 *  The following functions deal with the user-facing malloc implementation.
 */

/**
 * @brief
 *      Initialize first space from heap for later mm_malloc use 
 *
 * 1. Reset all list-head ptr
 * 2. Allocate and initialize Prologue/Epilogue 
 * 3. Allocate CHUNKSIZE memory space from heap
 * 4. Refer Fig. 9.42 on text book p. 863 for a graph demo of heap head 
 *
 * @note 
 *      1. Update global variable heap_listp
 *
 * @ret -1 on error, 0 on success.
 */
int mm_init(void) {

    // init list head 
    reset_all_list_head();

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }

    /* Init Prologue/Epilogue */
    set_word_val((unsigned int *)heap_listp, 0);    // alignment padding 
    heap_listp += WSIZE;                            // Prologue header 
    set_word_val((unsigned int *)heap_listp, pack_size(DSIZE, 1));    
    heap_listp += WSIZE;                            // Prologue footer  
    set_word_val((unsigned int *)heap_listp, pack_size(DSIZE, 1));    
    heap_listp += WSIZE;                            // Prologue footer  
    set_word_val((unsigned int *)heap_listp, pack_size(0, 1));    

    // adjust pointer to comlete init 
    heap_listp -= WSIZE;       

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (NULL ==  extend_heap(CHUNKSIZE/WSIZE)) {
	return -1;
    }

    return 0;
}


/**
 * @brief 
 *      Allocate a block with at least size bytes of payload
 *
 * Do following steps
 *      1. check heap correctness and handle special case 
 *      2. adjust size because we have minimum size requirement for 
 *         each block
 *      3. find from free lists to see if available
 *      4. if found, allocate space by calling place()
 *      5. if not found, extend heap and allocate space
 *
 * @param
 *      size: number of bytes to be allocate 
 * @ret 
 *      NULL if size == 0 or malloc failed 
 *      valid ptr if malloc success 
 */
void *malloc (size_t size) {
    size_t asize;      /// Adjusted block size 
    size_t extendsize; /// Amount to extend heap if no fit 
    char *bp;      

    checkheap(0);      // Let's make sure the heap is ok!

    // mem init at first time 
    if (heap_listp == 0){
	mm_init();
    }
 
    // check corner case 
    if (size == 0) {
        return NULL;
    }

    // Adjust block size to include overhead and alignment reqs
    if (size <= 2*DSIZE){  // minimum size = 24
	asize = MIN_BLK_SIZE; // 3*DSIZE
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    // first fit search 
    if ( NULL != (bp = find_fit(asize))) {
        place (bp, asize);
        return bp;
    }

    // if not fit, get more memory
    extendsize = ( asize > CHUNKSIZE ) ? asize : CHUNKSIZE;
    if ( NULL == ( bp = extend_heap(extendsize/WSIZE))){ // get memory failed
        return NULL;
    }
   
    // get extended memory, place and return 
    place(bp, asize);
    
    return bp;
}

/**
 * @brief 
 *      free a block
 *
 * Just reset header/footer and then call coalesce 
 * to merge freed blocks (if any)
 *
 * @note 
 *      call coalesce immediately
 * @param 
 *      ptr: pointer of a memory block to be freed 
 */
void free (void *ptr) {
    size_t size;

    // check corner case 
    if (ptr == NULL) {
        return;
    }
        
    if (heap_listp == 0) {
        mm_init();
        fprintf(stderr, "memory is not init when free() is called");
        return;
    }

    // check if the ptr is valid 
    if (!in_heap(ptr)){
        fprintf(stderr, "Error, the addr to be free is invalid: %lx\n", \
        (long)ptr);
        return;
    }

    size = get_size(get_header_ptr(ptr));

    // begin free 
    set_word_val(get_header_ptr(ptr), pack_size(size, 0));
    set_word_val(get_footer_ptr(ptr), pack_size(size, 0));

    // merge free memory, if any 
    coalesce(ptr);
}

/** 
 * @brief
 *      realloc a memory space with size byts for oldptr
 * @note
 *      1. will do memory content copy 
 *      2. old memory space will be freed
 * @param
 *      oldptr: pointer to old memory space 
 *      size: new size in terms of bytes to be allocated
 *
 * @return
 *      0 if input size == 0 or realloc failed
 *      valid pointer of newly allocated memory 
 */
void *realloc(void *oldptr, size_t size) {
    size_t oldsize;
    void *newptr;
    
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        free(oldptr);
        return 0;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if(oldptr == NULL) {
        return malloc(size);
    }
    
    newptr = malloc(size);
    
    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }
    
    /* Copy the old data. */
    oldsize = get_size(get_header_ptr(oldptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);
    
    /* Free the old block. */
    free(oldptr);
    
    return newptr;
}

/**
 * @brief   
 *      an user version of calloc, man calloc for detail
 * @param  
 *      follow standard input spec of calloc
 * @ret
 *      1. a valid ptr to memory space 
 *      2. 0 if failed of size == 0
 */
void *calloc (size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/**
 * @brief
 *      check if a block is valid 
 * @note 
 *      check
 *      1. alignment
 *      2. if block is in heap
 *      3. if header is matched with footer
 *      4. if size >= MIN_BLK_SIZE (24)
 *      5. if two joint freed block because we coalesced immediately
 * @param
 *      block pointer to be checked 
 * @attention
 *      will not terminate program when error encountered, just 
 *      print error msg
 */
static void checkblock(void *bp) 
{
    // check alignment
    if ((size_t)bp % DSIZE){
        printf("In function checkblock():");
	printf("Error: block[%p] is not doubleword aligned\n", bp);
    }

    // each block should be within heap
    if (!in_heap(bp)){
        printf("In function checkblock():");
	printf("Error: block[%p] is not in heap\n", bp);
    } 

    // check header/footer correctness
    if (get_word_val(get_header_ptr(bp)) != get_word_val(get_footer_ptr(bp))){
        printf("In function checkblock():");
	printf("Error: block[%p] header does not match footer\n", bp);
    }
    
    // check if size >= MIN size 
    if (get_size(get_header_ptr(bp)) < MIN_BLK_SIZE){
        if ( bp != heap_listp) {  // heap_listp can waive this check 
            printf("In function checkblock():");
	    printf("Error: block[%p] size is too small\n", bp);
	}
    }

    // check if two joint freed blocked existed
    if (!get_alloc(get_header_ptr(bp))){
        if (!get_alloc(get_header_ptr(get_next_blk_ptr(bp)))){
            printf("In function checkblock():");
	    printf("Error: block[%p] and its following block", bp);
            printf("are both free but not being coalesced\n");
        }
    } 
}

/**
 * @brief
 *      check if a list is valid 
 * @note
 *      Use given list_index to find a list-head 
 *      traverse taget list from head to tail, for each node
 *      check: 
 *      1. All next/previous pointers are consistent
 *      2. if block is in heap
 *      3. if block is freed
 *      4. if block fall to correct list 
 * @param
 *      list_index of free-list to be checked  
 * @attention
 *      will not terminate program when error encountered, just 
 *      print error msg
 * @ret
 *      number of frees blocks in this list
 */
static size_t check_one_list(size_t list_index){
    size_t list_size = 0;
    char *bp = get_list_head_by_index(list_index);
    char *bp_next;

    // traverse the list from begin to end 
    for(; bp != NULL; bp = get_succ_ptr(bp)) {
        list_size++; // count freed number
        bp_next = get_succ_ptr(bp);
    
        // check if bp_next's previous is bp
        if (NULL != bp_next){
            if (bp != get_pred_ptr(bp_next)){
                printf("In function check_one_list():");
	        printf("Error: block[%p]'s next ptr's previous\
	                ptr is not block[%p]", bp, bp);
            }
        }

        // check if bp is in heap
        if (!in_heap(bp)){
            printf("In function check_one_list():");
	    printf("Error: block[%p] is not in heap\n", bp);
        } 

        // check if bp is freed
        if (get_alloc(get_header_ptr(bp))){
            printf("In function check_one_list():");
	    printf("Error: block[%p] is in free-list but not freed\n", bp);
        }

        // check if bp is fall within correct list 
        if (get_list_index_by_size(get_size(get_header_ptr(bp)))\
                != list_index){
            printf("In function check_one_list():");
	    printf("Error: block[%p] should not in list[%zu]\n",\
	           bp, list_index);
        }

    }
    return list_size;
}

/**
 * @brief
 *      check if current heap is valid or not
 *
 * Do following steps
 *      1. check prologue
 *      2. check if heap_listp if valid 
 *      3. start from heap_listp, check every block correctness 
 *         by calling checkblock(), also count number of freed blocks
 *      4. check epilogue
 *      5. examines all lists' correctness by calling check_one_list
 *      6. examines if number of freed blocks are matched
 *
 * @param 
 *      1 to print debug info, 0 not to print 
 * @ret
 *      Returns 0 if no errors were found, otherwise returns the error
 */
int mm_checkheap(int verbose) {
    char *bp = heap_listp;
    size_t i;
    // number of free blocks by checking whole heap
    size_t no_free_blk_count_by_whole_heap = 0;  
    // number of free blocks by checking each lists
    size_t no_free_blk_count_by_lists = 0;

    // welcome msg
    if (verbose)
	printf("Heap (%p):\n", heap_listp);

    // check prologue
    if (get_size(get_header_ptr(heap_listp)) != DSIZE || \
        !get_alloc(get_header_ptr(heap_listp))){
        printf("Bad prologue header\n");
        return 1;
    }
    
    // check if heap_listp is valid
    checkblock(heap_listp);

    // from heap_listp to the end of heap,
    // check each following blocks' correctness
    for ( bp = heap_listp; get_size(get_header_ptr(bp)) > 0; \
          bp = get_next_blk_ptr(bp)){
        checkblock(bp); 
        if (!get_alloc(get_header_ptr(bp))){
            no_free_blk_count_by_whole_heap++;
        }
    }
   
    // check epilogue
    if ((get_size(get_header_ptr(bp)) != 0) || \
        !get_alloc(get_header_ptr(bp))){
   	printf("Bad epilogue header\n");
        return 1; 
    }

    // begin to check lists
    for ( i = 0; i < NO_OF_SIZE_TYPE; i++){ 
        no_free_blk_count_by_lists += check_one_list(i);
    }

    // check if free blk counts equal by two approaches
    if (no_free_blk_count_by_whole_heap != no_free_blk_count_by_lists){
        printf("Number of freed blocks mismatch\n");
    }

    return 0;
}

/** Additional help functions */

/**
 * @brief
 *      Extend heap withe free block and return its block pointer
 * 1. Call mem_sbrk to extend heap 
 * 2. The extended space will be formed as a new freed block 
 * 3. call coalesce() to see if can be merged, note that coalesce 
 *    function will then insert block to target list by its size
 *
 * @param 
 *      words: size to be extended in terms of bytes
 * @ret
 *      smallest pointer of available(freed) block 
 *      NULL if heap extension failed 
 */
static void *extend_heap(size_t words){
    char *bp; 
    size_t size;

    // adjust size to meet alignment requirement 
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 
    if ((bp = mem_sbrk(size)) == (void *)-1) {
	return NULL;                 
    }
    
    /* now we have a fresh space without init, hence, init to let 
       memory allocate can recognize */   
    // init header for free block 
    set_word_val(get_header_ptr(bp), pack_size(size, 0));
    // init footer for free block 
    set_word_val(get_footer_ptr(bp), pack_size(size, 0));
    // init epilogue header
    set_word_val(get_header_ptr(get_next_blk_ptr(bp)), pack_size(0, 1));

    /* Coalesce if the previous block was free */   
    return coalesce(bp);
}

/**
 * @brief 
 *      Boundary tag coalescing. Return ptr to coalesced block
 * 
 * 1. Follow text book to do the merge
 * 2. after merged, insert to front of a specific list
 *
 * @note 
 *      See fig. 9.40 in p. 860 for each case's definition
 *      also do linked-list node add/remove handeling
 * @param 
 *      bp: pointer of input block to be coalesced
 * @ret
 *      a new valid ptr after coalesced
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = get_alloc(get_footer_ptr(get_prev_blk_ptr(bp)));
    size_t next_alloc = get_alloc(get_header_ptr(get_next_blk_ptr(bp)));
    size_t size = get_size(get_header_ptr(bp));

    if (prev_alloc && next_alloc) {             // case 1
        ; // do nothing
    } else if (prev_alloc && !next_alloc) {     // case 2
        size += get_size(get_header_ptr(get_next_blk_ptr(bp)));
        remove_from_list(get_next_blk_ptr(bp));
        set_word_val(get_header_ptr(bp), pack_size(size, 0));
        set_word_val(get_footer_ptr(bp), pack_size(size, 0));
    } else if (!prev_alloc && next_alloc) {     // case 3
        remove_from_list(get_prev_blk_ptr(bp));
        size += get_size(get_header_ptr(get_prev_blk_ptr(bp)));
        set_word_val(get_footer_ptr(bp), pack_size(size, 0));
        bp = get_prev_blk_ptr(bp);
        set_word_val(get_header_ptr(bp), pack_size(size, 0));
    } else { //  !prev_alloc && !next_alloc     // case 4
        remove_from_list(get_next_blk_ptr(bp));
        remove_from_list(get_prev_blk_ptr(bp));
        size += get_size(get_header_ptr(get_next_blk_ptr(bp)));
        size += get_size(get_header_ptr(get_prev_blk_ptr(bp)));
        set_word_val(get_footer_ptr(get_next_blk_ptr(bp)), \
                     pack_size(size, 0));
        bp = get_prev_blk_ptr(bp);
        set_word_val(get_header_ptr(bp), pack_size(size, 0));
    }

    list_push_front(bp);
    return bp;
}

/**
 * @brief
 *      Find next free & big enough space's ptr 
 * @note
 *      1. according to its size, get a minimum list_index to 
 *         start with 
 *      2. if finding on current list is failed, find it on
 *         next list until no other list remainded
 * @param
 *      asize: a minimum size of be allocated 
 * @ret
 *      NULL if not found 
 *      valid pointer if found 
 */
static void *find_fit(size_t asize) {
    char *bp;
    size_t i = get_list_index_by_size(asize);

    for (; i < NO_OF_SIZE_TYPE; i++) {
        bp = find_fit_from_list(asize, i);
        if ( NULL != bp ){
            return bp;
        } 
    } 
    return NULL;
}
/**
 * @brief
 *      Find a big enough block in target list 
 * @note
 *      1. get target list by its list_index
 *      2. traverse from begin to end to find a big enough block
 * @param
 *      asize: a minimum size of be allocated 
 * @ret
 *      NULL if not found 
 *      valid pointer if found 
 */
static void *find_fit_from_list(size_t asize, size_t list_index) {
    void *bp = get_list_head_by_index(list_index);

    for(; bp != NULL; bp = get_succ_ptr(bp)) {
        ENSURES(!get_alloc(get_header_ptr(bp)));
        if (asize <= get_size(get_header_ptr(bp))) {
            return bp; 
        }
    }
    return NULL;
}

/**
 * @brief 
 *      place block of asize bytes at start of free block bp  
 *      amd split if remainder would be at least minimum block size
 * @note 
 *      Also do liked-list node add/remove handeling
 * @param 
 *      bp: pointer of memory block to be place 
 *      asize: size of target to be placed memory block
 */
static void place(void *bp, size_t asize) {
    size_t free_size = get_size(get_header_ptr(bp));

    remove_from_list(bp);

    if ((free_size - asize) >= (MIN_BLK_SIZE)){ // can be split
        // allocate asize 
        set_word_val(get_header_ptr(bp), pack_size(asize, 1));
        set_word_val(get_footer_ptr(bp), pack_size(asize, 1));
        // split block 
        bp = get_next_blk_ptr(bp);
        set_word_val(get_header_ptr(bp), pack_size(free_size - asize, 0));
        set_word_val(get_footer_ptr(bp), pack_size(free_size - asize, 0));
        // add to list
        list_push_front(bp);
    } else {
        set_word_val(get_header_ptr(bp), pack_size(free_size, 1));
        set_word_val(get_footer_ptr(bp), pack_size(free_size, 1));
    }
}

/**
 * @brief   
 *      push one note at front of list
 *
 * 1. Find target list by its size 
 * 2. Insert at font of target list 
 *
 * @param 
 *      bp: pointer of memory block to be inserted  
*/
static void list_push_front(char *bp) {
    int size = get_size(get_header_ptr(bp));
    char **list_head = get_list_head_by_size(size);
    // bp's pred should be NULL 
    set_pred_ptr(bp, NULL);

    if (NULL != *list_head) { // target list is not empty
        // set old-head's pred to bp 
        set_pred_ptr(*list_head, bp);
    }

    // let bp's succ to old_head
    set_succ_ptr(bp, *list_head);

    // update list head
    *list_head = bp;
}

/**
 * @brief 
 *      remove one node from list
 * 1. Find target list by its size 
 * 2. Remove the node from target list
 * @param 
 *      bp: pointer of memory block to be inserted  
 */ 
static void remove_from_list(char *bp) {    
    int size = get_size(get_header_ptr(bp));
    char **list_head = get_list_head_by_size(size);
    char *pred_ptr = get_pred_ptr(bp);
    char *succ_ptr = get_succ_ptr(bp);

    // corner case, if there's only one remained 
    if ((NULL == pred_ptr) && (NULL == succ_ptr)){
        *list_head = NULL;
        return;
    }

    if ((NULL == pred_ptr) && (NULL != succ_ptr)){ // head
        ENSURES(!get_alloc(get_header_ptr(succ_ptr)));
        *list_head = succ_ptr;
        set_pred_ptr(succ_ptr, NULL);
        return;
    }

    if ((NULL != pred_ptr) && (NULL == succ_ptr)){ // tail
        ENSURES(!get_alloc(get_header_ptr(pred_ptr)));
        set_succ_ptr(pred_ptr, NULL);
        return;
    }

    if ((NULL != pred_ptr) && (NULL != succ_ptr)){ // middle
        ENSURES(!get_alloc(get_header_ptr(succ_ptr)));
        ENSURES(!get_alloc(get_header_ptr(pred_ptr)));
        set_pred_ptr(succ_ptr, pred_ptr);
        set_succ_ptr(pred_ptr, succ_ptr);
    }
}
