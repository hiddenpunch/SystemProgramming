//--------------------------------------------------------------------------------------------------
// System Programming                       Memory Lab                                   Fall 2020
//
/// @file
/// @brief dynamic memory manager
/// @author <yourname>
/// @studid <studentid>
//--------------------------------------------------------------------------------------------------


// Dynamic memory manager
// ======================
// This module implements a custom dynamic memory manager.
//
// Heap organization:
// ------------------
// The data segment for the heap is provided by the dataseg module. A 'word' in the heap is
// eight bytes.
//
// Implicit free list:
// -------------------
// - minimal block size: 32 bytes (header +footer + 2 data words)
// - h,f: header/footer of free block
// - H,F: header/footer of allocated block
//
// - state after initialization
//
//         initial sentinel half-block                  end sentinel half-block
//                   |                                             |
//   ds_heap_start   |   heap_start                         heap_end       ds_heap_brk
//               |   |   |                                         |       |
//               v   v   v                                         v       v
//               +---+---+-----------------------------------------+---+---+
//               |???| F | h :                                 : f | H |???|
//               +---+---+-----------------------------------------+---+---+
//                       ^                                         ^
//                       |                                         |
//               32-byte aligned                           32-byte aligned
//
// - allocation policies: first, next, best fit
// - block splitting: always at 32-byte boundaries
// - immediate coalescing upon free
//


#include <assert.h>
#include <error.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dataseg.h"
#include "memmgr.h"

void mm_check(void);


static void *ds_heap_start = NULL;                     ///< physical start of data segment
static void *ds_heap_brk   = NULL;                     ///< physical end of data segment
static void *heap_start    = NULL;                     ///< logical start of heap
static void *heap_end      = NULL;                     ///< logical end of heap
static void *recent_block  = NULL;                     ///< the block accessed latest
static int  PAGESIZE       = 0;                        ///< memory system page size
static int  mm_initialized = 0;                        ///< initialized flag (yes: 1, otherwise 0)
static int  mm_loglevel    = 0;                        ///< log level (0: off; 1: info; 2: verbose)
static void* (*get_block)(size_t size) = NULL;  // <-- this is a function pointer
                                        // it can point to any function that
                                        // - returns void*
                                        // - takes one size_t argument
#define MAX(a, b)          ((a) > (b) ? (a) : (b))     ///< MAX function

#define TYPE               unsigned long               ///< word type of heap
#define TYPE_SIZE          sizeof(TYPE)                ///< size of word type

#define ALLOC              1                           ///< block allocated flag
#define FREE               0                           ///< block free flag
#define STATUS_MASK        ((TYPE)(0x7))               ///< mask to retrieve flagsfrom header/footer
#define SIZE_MASK          (~STATUS_MASK)              ///< mask to retrieve size from header/footer

#define CHUNKSIZE          (1*(1 << 12))               ///< size by which heap is extended
#define TO_CHUNKSIZE(size) (((size-1)/CHUNKSIZE+1)*CHUNKSIZE) //< minimum size by which heap is extended over 'size'

#define BS                 32                          ///< minimal block size. Must be a power of 2
#define BS_MASK            (~(BS-1))                   ///< alignment mask

#define ROUND_UP(w)        (((w)+BS-1)/BS*BS)          ///< round up to next multiple of block size
#define ROUND_DOWN(w)      ((w)/BS*BS)                 ///< round down to previous multiple of block size

#define WORD(p)            ((TYPE)(p))                 ///< convert pointer to TYPE
#define PTR(w)             ((void*)(w))                ///< convert TYPE to void*

#define PREV_PTR(p)        ((p)-TYPE_SIZE)             ///< get pointer to word preceeding p

#define PACK(size,status)  ((size) | (status))         ///< pack size & status into boundary tag
#define SIZE(v)            (v & SIZE_MASK)             ///< extract size from boundary tag
#define STATUS(v)          (v & STATUS_MASK)           ///< extract status from boundary tag

#define GET(p)             (*(TYPE*)(p))               ///< read word at *p
#define GET_SIZE(p)        (SIZE(GET(p)))              ///< extract size from header/footer
#define GET_STATUS(p)      (STATUS(GET(p)))            ///< extract status from header/footer

#define PUT(p, v)          (*((TYPE*)(p)) = (v))       ///< write value v to defreference pointer *p

#define HDR2FTR(p)         ((p)+GET_SIZE(p)-TYPE_SIZE) ///< get location of footer tag given a header tag

#define NEXT_BLOCK(p)      ((p)+GET_SIZE(p))           ///< get pointer to next block of p
#define PREV_BLOCK(p)      ((p)-GET_SIZE((p)-TYPE_SIZE)) ///< get pointer to previous block of p
// TODO add more macros as needed

/// @brief print a log message if level <= mm_loglevel. The variadic argument is a printf format
///        string followed by its parametrs
#define LOG(level, ...) mm_log(level, __VA_ARGS__)

/// @brief print a log message. Do not call directly; use LOG() instead
/// @param level log level of message.
/// @param ... variadic parameters for vprintf function (format string with optional parameters)
static void mm_log(int level, ...)
{
  if (level > mm_loglevel) return;

  va_list va;
  va_start(va, level);
  const char *fmt = va_arg(va, const char*);

  if (fmt != NULL) vfprintf(stdout, fmt, va);

  va_end(va);

  fprintf(stdout, "\n");
}


/// @brief print error message and terminate process. The variadic argument is a printf format
///        string followed by its parameters
#define PANIC(...) mm_panic(__func__, __VA_ARGS__)

/// @brief print error message and terminate process. Do not call directly, Use PANIC() instead.
/// @param func function name
/// @param ... variadic parameters for vprintf function (format string with optional parameters)
static void mm_panic(const char *func, ...)
{
  va_list va;
  va_start(va, func);
  const char *fmt = va_arg(va, const char*);

  fprintf(stderr, "PANIC in %s%s", func, fmt ? ": " : ".");
  if (fmt != NULL) vfprintf(stderr, fmt, va);

  va_end(va);

  fprintf(stderr, "\n");

  exit(EXIT_FAILURE);
}

static void *ff_get_free_block(size_t size);
static void *nf_get_free_block(size_t size);
static void *bf_get_free_block(size_t size);

void mm_init(AllocationPolicy ap)
{
  LOG(1, "mm_init(%d)", ap);

  switch(ap) {
    case ap_FirstFit: get_block = ff_get_free_block;break;
    case ap_NextFit: get_block = nf_get_free_block;break;
    case ap_BestFit: get_block = bf_get_free_block;break;
    default: PANIC("Invalid allocation policy.");
  }
  //
  // retrieve heap status and perform a few initial sanity checks
  //
  ds_heap_stat(&ds_heap_start, &ds_heap_brk, NULL);
  PAGESIZE = ds_getpagesize();

  LOG(2, "  ds_heap_start    %p\n"
         "  ds_heap_brk      %p\n"
         "  PAGESIZE         %d\n",
         ds_heap_start, ds_heap_brk, PAGESIZE);

  if (ds_heap_start == NULL) PANIC("Data segment not initialized.");
  if (ds_heap_start != ds_heap_brk) PANIC("Heap not clean.");
  if (PAGESIZE == 0) PANIC("Reported pagesize == 0.");

  //
  // initialize heap
  //
  // TODO
  // get first chunk of memory for heap
  LOG(2, "Get first block of memory for heap");
  if(ds_sbrk(CHUNKSIZE) == (void*)-1) PANIC("Cannot increase heap break");
  ds_heap_brk = ds_sbrk(0);
  LOG(2, "Yay, Break is now at %p", ds_heap_brk);
  // compute location of heap_start/end (32 bytes from start/end of ds_heap_start/brk)
  heap_start = PTR((WORD(ds_heap_start)+TYPE_SIZE+BS-1)/BS*BS);
  heap_end = PTR((WORD(ds_heap_brk)-TYPE_SIZE)/BS*BS);

  LOG(2, "heap start at %p\n"
        "heap end at %p\n",
        heap_start, heap_end);
  // write initial sentinel half block
  TYPE F = PACK(0, ALLOC);
  PUT(heap_start-TYPE_SIZE, F);

  TYPE H = PACK(0, ALLOC);
  PUT(heap_end, H);
  
  // write free block
  TYPE size = heap_end-heap_start;
  TYPE bdrytag = PACK(size, FREE);

  PUT(heap_start, bdrytag);
  PUT(heap_end-TYPE_SIZE, bdrytag);

  recent_block = heap_start;
  //
  // heap is initialized
  //
  mm_initialized = 1;
}

static void* nf_get_free_block(size_t size)
{
  LOG(1, "nf_get_free_block(0x%lx (%lu))", size, size);

  assert(mm_initialized);
  
  void *block = recent_block;
  size_t bsize, bstatus;
  LOG(2, "  starting search at %p", block);
  do{
    bstatus = GET_STATUS(block);
    bsize = GET_SIZE(block);
    LOG(2, "  %p: size: %lx (%lu), status: %s", block, bsize, bsize, bstatus == ALLOC ? "allcated" : "free");

    if((bstatus == FREE) && (bsize >= size)) {
      //found block
      LOG(2, " --> match");
      recent_block = block;// this block will be used right after now
      return block;
    }
    
    block += bsize;
    if(GET_SIZE(block)==0) block = heap_start;//if block arrives at heap_end, return to heap_start
  } while(block!=recent_block);//travel once all block => cannot find block

  LOG(2, "no suitable block found");
  return NULL;
}
static void* bf_get_free_block(size_t size)
{
  LOG(1, "bf_get_free_block(0x%lx (%lu))", size, size);

  assert(mm_initialized);

  void *block = heap_start;
  void *bf_block = NULL;
  size_t bsize, bstatus, bf_size=-1;
  LOG(2, "  starting search at %p", block);
  do{
    bstatus = GET_STATUS(block);
    bsize = GET_SIZE(block);

    LOG(2, "  %p: size: %lx (%lu), status: %s", block, bsize, bsize, bstatus == ALLOC ? "allocated" : "free");
    if((bstatus == FREE) && (bsize >= size)) {
      //found qualified block
      if(bf_size==-1 || bsize<bf_size){// until this block, this block is the best
        bf_block = block;
        bf_size = bsize;
      }
    }
    
    block += bsize;
  } while(GET_SIZE(block)>0);

  if(bf_size==-1){
    LOG(2, "  no suitable block found");
    return NULL;
  } else {
    LOG(2, " --> match");
    return bf_block;
  }
}
static void* ff_get_free_block(size_t size)
{
  LOG(1, "ff_get_free_block(0x%lx (%lu))", size, size);

  assert(mm_initialized);

  void *block = heap_start;
  size_t bsize, bstatus;
  LOG(2, "  starting search at %p", block);
  do{
    bstatus = GET_STATUS(block);
    bsize = GET_SIZE(block);

    LOG(2, "  %p: size: %lx (%lu), status: %s", block, bsize, bsize, bstatus == ALLOC ? "allocated" : "free");

    if((bstatus == FREE) && (bsize >= size)) {
      // found block
      LOG(2, "  --> match");
      return block;
    }
    
    // next block
    block += bsize;

  } while (GET_SIZE(block)>0); // only the sentinel at the end has size == 0

  // no block found
  LOG(2, " no suitable block found");
  return NULL;

}

static void coalesce(void *block)
{
  LOG(1, "coalesce(%p)", block);
  assert(mm_initialized);
  assert(GET_STATUS(block) == FREE);

  TYPE size = GET_SIZE(block);
  void *hdr = block;
  void *ftr = HDR2FTR(hdr);
 // recent_block = block;// recently touched block

  // can we coalesce with following block?
  if(GET_STATUS(NEXT_BLOCK(block)) == FREE) {
    LOG(2, "  coalescing with suceeding block");
    // size of coalesced block: size of block + size of following block
    size += GET_SIZE(NEXT_BLOCK(block));
    // compute new location of footer tag of coalesced block
    ftr = hdr + size - TYPE_SIZE;
    if(NEXT_BLOCK(block)==recent_block) recent_block = block+size;
  }
  

  // can we coalesce with preceeding block?
  if(GET_STATUS(block-TYPE_SIZE) == FREE) {
    LOG(2, "  coalescing with previous block");

    // size of coalesced block: size + size of preceeding block
    size += GET_SIZE(PREV_BLOCK(block));
    // compute new location of header tag of coalesced block
    hdr = PREV_BLOCK(block);
    if(block == recent_block) recent_block = block+size;// recently touched block
  }

  if(size > GET_SIZE(block)) {// if coalesced, add new hdr, ftr
    PUT(hdr, PACK(size, FREE));
    PUT(ftr, PACK(size, FREE));
  }
}

void* expand_heap(size_t blocksize){
  if(blocksize<CHUNKSIZE){
    blocksize = CHUNKSIZE;
  }
  if(ds_sbrk(blocksize)==(void*)-1) PANIC("not enouth memory");
  ds_heap_brk = ds_sbrk(0);
  heap_end = PTR((WORD(ds_heap_brk)-TYPE_SIZE)/BS*BS);
  TYPE H = PACK(0, ALLOC);
  PUT(heap_end, H);
  
  void *prev_heap_end = PTR(WORD(heap_end)-blocksize);
  TYPE size = heap_end-prev_heap_end;
  TYPE bdrytag = PACK(size, FREE);

  PUT(prev_heap_end, bdrytag);
  PUT(heap_end-TYPE_SIZE, bdrytag);
  
  coalesce(prev_heap_end);
  return PREV_BLOCK(heap_end);
}

void* mm_malloc(size_t size)
{
  LOG(1, "mm_malloc(0x%lx (%lu))", size, size);

  assert(mm_initialized);

  //
  // TODO
  //
  
  // compute block size (header + footer + payload, round up to BS)
  size_t blocksize = ROUND_UP(TYPE_SIZE + size + TYPE_SIZE);
  LOG(2, "  blocksize:    %lx (%lu)", blocksize, blocksize);

  // find free block
  void *block = get_block(blocksize);

  LOG(2, "  got free block: %p", block);
  
  if(block == NULL) {
    // no free block is big enough -> expand heap
    int request_size = blocksize;
    if(GET_STATUS(heap_end-TYPE_SIZE)==FREE){// if last block if free, we can utilize this space
     request_size=request_size-GET_SIZE(heap_end-TYPE_SIZE);
    }
    block = expand_heap(TO_CHUNKSIZE(request_size));
  }

  // split block
  size_t bsize = GET_SIZE(block);
  if(blocksize < bsize) {
    void *next_block = block + blocksize;
    size_t next_size = bsize - blocksize;

    PUT(next_block, PACK(next_size, FREE)); // header of next_block
    PUT(next_block + next_size - TYPE_SIZE, PACK(next_size, FREE));
  }

  PUT(block, PACK(blocksize, ALLOC));
  PUT(block + blocksize - TYPE_SIZE, PACK(blocksize, ALLOC));
  return block + TYPE_SIZE;
}

void* mm_calloc(size_t nmemb, size_t size)
{
  LOG(1, "mm_calloc(0x%lx, 0x%lx)", nmemb, size);

  assert(mm_initialized);

  //
  // calloc is simply malloc() followed by memset()
  //
  void *payload = mm_malloc(nmemb * size);

  if (payload != NULL) memset(payload, 0, nmemb * size);

  return payload;
}

void* mm_realloc(void *ptr, size_t size)
{
  LOG(1, "mm_realloc(%p, 0x%lx)", ptr, size);

  assert(mm_initialized);

  //
  // TODO (optional)
  //

  return NULL;
}

void mm_free(void *ptr)
{
  LOG(1, "mm_free(%p)", ptr);

  assert(mm_initialized);

  //
  // TODO
  //
  void *block = ptr - TYPE_SIZE;

  // check whether block is allocated
  if(GET_STATUS(block) != ALLOC) {
    //LOG(1, "  WARNING: double-free detected");
    PANIC(" WARNING: double-free detected");
    return;
  }

  // mark as free
  TYPE size = GET_SIZE(block);
  PUT(block, PACK(size, FREE));
  PUT(block+size-TYPE_SIZE, PACK(size, FREE));

  // coalesce
  coalesce(block);
}


void mm_setloglevel(int level)
{
  mm_loglevel = level;
}


void mm_check(void)
{
  assert(mm_initialized);

  void *p;

  printf("\n----------------------------------------- mm_check ----------------------------------------------\n");
  printf("  ds_heap_start:          %p\n", ds_heap_start);
  printf("  ds_heap_brk:            %p\n", ds_heap_brk);
  printf("  heap_start:             %p\n", heap_start);
  printf("  heap_end:               %p\n", heap_end);
  printf("\n");
  p = PREV_PTR(heap_start);
  printf("  initial sentinel:       %p: size: %6lx, status: %lx\n", p, GET_SIZE(p), GET_STATUS(p));
  p = heap_end;
  printf("  end sentinel:           %p: size: %6lx, status: %lx\n", p, GET_SIZE(p), GET_STATUS(p));
  printf("\n");
  printf("  blocks:\n");

  long errors = 0;
  p = heap_start;
  while (p < heap_end) {
    TYPE hdr = GET(p);
    TYPE size = SIZE(hdr);
    TYPE status = STATUS(hdr);
    printf("    %p: size: %6lx, status: %lx\n", p, size, status);

    void *fp = p + size - TYPE_SIZE;
    TYPE ftr = GET(fp);
    TYPE fsize = SIZE(ftr);
    TYPE fstatus = STATUS(ftr);

    if ((size != fsize) || (status != fstatus)) {
      errors++;
      printf("    --> ERROR: footer at %p with different properties: size: %lx, status: %lx\n", 
             fp, fsize, fstatus);
    }

    p = p + size;
    if (size == 0) {
      printf("    WARNING: size 0 detected, aborting traversal.\n");
      break;
    }
  }

  printf("\n");
  if ((p == heap_end) && (errors == 0)) printf("  Block structure coherent.\n");
  printf("-------------------------------------------------------------------------------------------------\n");
}
