/** @file csim.c
 *  @brief A cache behavior simulator for CS:APP Cache Lab
 *
 *  Source file with my solution of Part (A) to this Lab.
 *  The spec and funtionality are described in writeup
 *
 *  @author Wei-Lin Tsai - weilints@andrew.cmu.edu
 *  @bug No known bugs.
 */

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<getopt.h>
#include<string.h>
#include"cachelab.h"    // for printSummary

/** Defines */
#define ADDR_WIDTH 64 
#define MAX_NAME_LEN 1024

/** Global variable */
static unsigned int hitCount = 0;                      ///< hit count
static unsigned int missCount = 0;                     ///< miss count
static unsigned int eviCount = 0;                      ///< eviction count

/** Structure define */
typedef struct cache_line {
    char  v;            ///< valid or not
    unsigned long tag;  ///< tag field for 64bits addr
    /* leave data field empty now here */
} CacheLine;

typedef struct cache_set {
    CacheLine *cLine;           ///< cache line array
    unsigned int noLine;        ///< E: number of sets per set
    /**array of LRU record. arrLRU[0] is most recently used,
     * arrLRU[noLine-1] is least recently used*/
    unsigned int *arrLRU;      
} CacheSet;

typedef struct cache {
    unsigned int bitSet;        ///< s: number of set index bits
    unsigned int noLine;        ///< E: number of lines per set 
    unsigned int bitBlock;      ///< b: number of block bits
    CacheSet *cSet;             ///< cache set array
} Cache;


/** functions declaration */
void ParseArgs (int argc, char **argv, Cache *inCache, char *inFileName );
void PrintErrMsgAndExit(char *errMsg);
void ConstructCacheIns( Cache *inCache);
FILE *OpenFile (char *inFileName);
void DoSim(Cache *inCache, FILE *inFptr);
void SimOneInstruction(Cache *inCache, char inType, unsigned long inAddr);
void SimOneLoadOrSave(CacheSet *inSet, unsigned long inTag);
void UpdateLRUWhenHit(unsigned int *inLRU, unsigned int inIndex,\
                      unsigned int sizeLRU); 
void UpdateLRUWhenMiss(unsigned int *inLRU,unsigned int inIndex,\
                       unsigned int sizeLRU); 
unsigned int UpdateLRUWhenEvic(unsigned int *inLRU, unsigned int sizeLRU);

/**
 * @brief main function
 *
 * Read the input and do cache simulation
 *
 * @note 
 *      Usage: ./csim -s <s> -E <E> -b <b> -t <tracefile>
 *      
 *      -s <s>: Number of set index bits (S = 2^s is the number of sets)
 *      -E <E>: Associativity (number of lines per set)
 *      -b <b>: Number of block bits (B = 2^b is the block size)
 *      -t <tracefile>: Name of the valgrind trace to replay
 */
int main(int argc, char **argv) {
    Cache cacheIns;                         ///< cache instance
    char fileNameTrace[MAX_NAME_LEN];       ///< file name of trace file 
    FILE *fptrTrace;                        ///< file ptr of trace file

    ParseArgs(argc, argv, &cacheIns, fileNameTrace);
    ConstructCacheIns(&cacheIns);

    fptrTrace =  OpenFile(fileNameTrace);
    DoSim(&cacheIns, fptrTrace);

    printSummary(hitCount, missCount, eviCount);

    // do not free cacheIns because the program is going to be terminated 
    fclose(fptrTrace);

    return 0;
}

/** function implementation */
/**
 * @brief 
 *     Parse args and do error handling
 * @note 
 *     Will run in batch mode, so do not support <-h>
 * @attention
 *     will print error msg and terminate directly when error occurs
 *
 * @param argc int from main
 * @param argv char** from main
 * @param inCache chace pointer 
 * @param inFileName will store trace file name
 */
void ParseArgs (int argc, char **argv, Cache *inCache, char *inFileName ){
    int opt;
    while ( -1 != (opt = getopt(argc, argv, "s:E:b:t:"))){
        switch(opt) {
            case 's':
                inCache->bitSet = atoi(optarg);
                break;
            case 'E':
                inCache->noLine = atoi(optarg);
                break;
            case 'b':
                inCache->bitBlock = atoi(optarg);
                break;
            case 't':
                strncpy(inFileName, optarg, MAX_NAME_LEN);
                break;
            default:
                PrintErrMsgAndExit("invalid arguments"); 
                break;
        }
    }
} 

/**
 * @brief Util function to print err msg and terminate 
 * @param errMsg, input err msg
 */
void PrintErrMsgAndExit(char *errMsg) {
    fprintf (stderr, "Error: %s\n. The program will terminate...", errMsg);
    exit(1);
}
/**
 * @brief construct cache instance 
 *
 * Memory allocation for cache instance 
 *
 * @note 
 *      because we use calloc instead of mallloc,
 *      all default values are set to 0
 * @attention
 *      will print error msg and terminate directly when memory
 *      allocation failed
 *
 * @param inCache, cache ptr to store allocated memory 
 * 
 */
void ConstructCacheIns( Cache *inCache) {
    unsigned int i;
    CacheSet *ptrSet;
    unsigned int noSet = 1U << (inCache->bitSet);      ///< number of sets
    unsigned int noLine = inCache->noLine;


    // allocate array of cache set 
    if (NULL == (inCache->cSet = calloc (noSet, sizeof (CacheSet)))) {
        PrintErrMsgAndExit("Memory allocation failed");
    }

    // for each set, allocate E lines
    for (i = 0; i < noSet; i++) {
        ptrSet = &inCache->cSet[i];
        if (NULL == (ptrSet->cLine = calloc (noLine, sizeof(CacheLine)))){
            PrintErrMsgAndExit("Memory allocation failed");
        }
        if (NULL == (ptrSet->arrLRU = calloc (noLine, sizeof(unsigned int)))){
            PrintErrMsgAndExit("Memory allocation failed");
        }
        ptrSet->noLine = noLine;
    }
}

/**
 * @brief open file and do error handling
 *
 * @attention
 *     because it will run in batch mode, will print error msg and terminate
 *     directly when file open failed 
 * @param inFileName, char * of trace file name 
 *
 * @return valid file ptr for read 
 */
FILE *OpenFile (char *inFileName){
    char errMsg[MAX_NAME_LEN];
    FILE *fptr;

    fptr = fopen (inFileName, "r");
    if ( NULL == fptr) {
        sprintf(errMsg, "Can not open file %s", inFileName);
        PrintErrMsgAndExit(errMsg);
    }
    return fptr;
}

/**
 * @brief 
 *    read data from file ptr and do the simulation
 *
 * @param inCache, cache ptr to cache instance
 * @param inFptr, file ptr for read
 */
void DoSim(Cache *inCache, FILE *inFptr) {
    char type;
    unsigned long addr;
    int size;
    while (fscanf(inFptr, " %c %lx, %d\n", &type, &addr, &size)>0){
        SimOneInstruction(inCache, type, addr);
    }
}

/**
 * @brief 
 *    simulate one instruction, skip I
 * 
 * @attention
 *    will terminate if type is invalid
 *
 * @param inCache, cache ptr to cache instance
 * @param inType, input type, can be L/S/M/I
 * @param inAddr, addr of target instruction
 */
void SimOneInstruction(Cache *inCache, char inType, unsigned long inAddr) {
    unsigned int setIndex;      ///< index of sets
    unsigned long tag;
   
    ///< bits number of tag
    unsigned int bitTag = ADDR_WIDTH - inCache->bitSet - inCache->bitBlock;

    tag = inAddr >> (inCache->bitSet + inCache->bitBlock);
    setIndex = (inAddr << bitTag) >> bitTag >> inCache->bitBlock; 
    
    if (inType == 'L'){
        SimOneLoadOrSave(&inCache->cSet[setIndex], tag);
    } else if (inType == 'S'){
        SimOneLoadOrSave(&inCache->cSet[setIndex], tag);
    } else if (inType == 'M'){
        // do twice to simulate modify operation
        SimOneLoadOrSave(&inCache->cSet[setIndex], tag);
        SimOneLoadOrSave(&inCache->cSet[setIndex], tag);
    } else if (inType == 'I'){
        // do nothing 
    } else {
        PrintErrMsgAndExit("Invalid instruction type");
    }
}

/**
 * @brief 
 *    simulate one instruction if it's load or save
 * 
 *
 * @param inCache, cache ptr to cache instance
 * @param inType, input type, can be L/S/M/I
 * @param inAddr, addr of target instruction
 */
void SimOneLoadOrSave(CacheSet *inSet, unsigned long inTag){
    unsigned int i;
    CacheLine *ptrTmpLine;
    int hitIndex = -1;           ///< -1 if miss, otherwise it's hit index
    char isFull = 1;             ///< if every one is valid, then it's full
// when miss/eviction, we need put a new line in inSet->cLine[candidateIndex]
    unsigned int candidateIndex; 
    unsigned int invalidIndex;   ///< can be used for miss

    // check if hit
    for (i = 0; i < inSet->noLine; i++){
        ptrTmpLine = &inSet->cLine[i];
        if (ptrTmpLine->v && (inTag == ptrTmpLine->tag)){
              hitIndex = i;
        }
        
        // if one of element is invalid, then this set is not full
        if (!ptrTmpLine->v){
            isFull = 0;
            invalidIndex = i;
        }
    }

    if ( hitIndex >= 0 ){ // hit
        hitCount++;
        UpdateLRUWhenHit(inSet->arrLRU, hitIndex, inSet->noLine);
    } else {              // not hit
        missCount++;
        if (isFull){      // eviction
            eviCount++;
            candidateIndex = UpdateLRUWhenEvic(inSet->arrLRU, inSet->noLine);
            inSet->cLine[candidateIndex].tag = inTag; // update tag
        } else {          // pure miss
            UpdateLRUWhenMiss(inSet->arrLRU, invalidIndex, inSet->noLine);
            // update v/tag as well
            inSet->cLine[invalidIndex].tag = inTag;
            inSet->cLine[invalidIndex].v = 1;
        }
    }
}

/**
 * @brief 
 *    Update LRU array when hit
 *
 * @param inLRU, ptr to LRU array
 * @param inIndex, the index to be update
 * @param sizeLRU, size of LRU
 */
void UpdateLRUWhenHit(unsigned int *inLRU, unsigned int inIndex,\
                      unsigned int sizeLRU){
    unsigned int i;
    unsigned int targetLocation;

    for (i = 0; i < sizeLRU; i++) {
        if (inLRU[i] == inIndex) {
            targetLocation = i;
            break;
        }    
    }

    // every element before targetLocation shift by one 
    // and let inLRU[0] = inIndex
    for (i = targetLocation; i > 0; i--) {
        inLRU[i] = inLRU[i-1];
    }
    inLRU[0] = inIndex;
}

/**
 * @brief 
 *    Update LRU array when miss
 *
 * @param inLRU, ptr to LRU array
 * @param inIndex, the index to be update
 * @param sizeLRU, size of LRU
 */
void UpdateLRUWhenMiss(unsigned int *inLRU, unsigned int inIndex,\
                       unsigned int sizeLRU) {
    unsigned int i;
    
    // all shift by one
    for ( i = sizeLRU -1; i > 0; i--) { // when  size == 1, do nothing
         inLRU[i] = inLRU[i-1];
    }
    inLRU[0] = inIndex;
} 

/**
 * @brief 
 *    Update LRU array when eviction
 *
 * @param inLRU, ptr to LRU array
 * @param sizeLRU, size of LRU
 * @return retIndex, the index to be evicted 
 */
unsigned int UpdateLRUWhenEvic(unsigned int *inLRU, unsigned int sizeLRU){
    unsigned int i;
    unsigned int retIndex = inLRU[sizeLRU-1];
 
    // all shift by one
    for ( i = sizeLRU -1; i > 0; i--) { // when  size == 1, do nothing
         inLRU[i] = inLRU[i-1];
    }
    inLRU[0] = retIndex;
   
    return retIndex;
}

