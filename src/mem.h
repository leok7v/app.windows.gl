#pragma once
#include "std.h"

// Memory API
BEGIN_C

#define stack_alloc(n) alloca((n))
#define stack_alloc_0(n) memset_(alloca((n)), 0, (n))
#define mem_alloc(n) malloc((n))
#define mem_realloc(a, n) realloc((a), (n))
#define mem_free(a) free((a))
#define mem_alloc_0(n) memset_(malloc(n), 0, (n))
#define mem_alloc_aligned(bytes, a) _aligned_malloc(bytes, a)
#define mem_free_aligned(p) { if (p != null) { _aligned_free(p); } }

/* Attempts to map entire content of specified existing
   non empty file not larger then 0x7FFFFFFF bytes
   into memory address in readonly mode.
   On success returns the address of mapped file
   on failure returns 0 (zero, aka NULL pointer).
   If 'bytes' pointer is not 0 it receives the number
   of mapped bytes equal to original file size. */
void* mem_map_read(const char* filename, int* bytes, bool read_only);
void* mem_map_read_write(const char* filename, int* bytes, bool read_only);

/* Unmaps previously mapped file */
void mem_unmap(void* address, int bytes);

inline void* memset_(void* a, int value, int bytes) { return a != null ? memset(a, value, bytes) : null; }

END_C



