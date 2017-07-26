#pragma once
#include "std.h"

// Memory API
BEGIN_C

#define mem_alloc(n) malloc(n)
#define mem_realloc(a, n) realloc(a, n)
#define mem_free(a) free(a)
#define mem_allocz(n) memset_(malloc(n), 0, n)
#define mem_alloc_aligned(bytes, a) _aligned_malloc(bytes, a)
#define mem_free_aligned(p) { if (p != null) { _aligned_free(p); } }

/* Attempts to map entire content of specified existing 
   non empty file not larger then 0x7FFFFFFF bytes
   into memory address in readonly mode.  
   On success returns the address of mapped file 
   on failure returns 0 (zero, aka NULL pointer).
   If 'bytes' pointer is not 0 it receives the number
   of mapped bytes equal to original file size. */
void* mem_map(const char* filename, int* bytes, bool read_only);

/* Unmaps previously mapped file */ 
void mem_unmap(void* address, int bytes);

END_C

#ifdef IMPLEMENT_MEM

BEGIN_C

inline void* memset_(void* a, int value, int bytes) {
    return a != null ? memset(a, value, bytes) : null;
}

#ifdef WINDOWS

void* mem_map(const char* filename, int* bytes, bool read_only) {
      void* address = null;
      HANDLE file = CreateFileA(filename, read_only ? GENERIC_READ : (GENERIC_READ|GENERIC_WRITE), 0, null, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, null);
      if (file != INVALID_HANDLE_VALUE) {
          LARGE_INTEGER size = {0};
          if (GetFileSizeEx(file, &size) && 0 < size.QuadPart && size.QuadPart <= 0x7FFFFFFF) {
              HANDLE map_file = CreateFileMapping(file, NULL, read_only ? PAGE_READONLY : PAGE_READWRITE, 0, (DWORD)size.QuadPart, null);
              if (map_file != null) {
                  address = MapViewOfFile(map_file, read_only ? FILE_MAP_READ : FILE_MAP_READ|SECTION_MAP_WRITE, 0, 0, (int)size.QuadPart);
                  if (address != null) {
                      *bytes = (int)size.QuadPart;
                  }
                  int b = CloseHandle(map_file);
                  assert(b); (void)b;
              }
          }
          int b = CloseHandle(file);
          assert(b); (void)b;
      }
      return address;
}

void mem_unmap(void* address, int bytes) {
    if (address != null) {
        int b = UnmapViewOfFile(address); (void)bytes; /* unused */
        assert(b); (void)b;
    }
}

#else

void* mem_map(const char* filename, int* bytes, bool readonly) {
    void* address = null;
    int fd = open(filename, O_RDONLY);
    if (fd >= 0) {
        int length = (int)lseek(fd, 0, SEEK_END);
        if (0 < length && length <= 0x7FFFFFFF) {
            address = mmap(0, length, PROT_READ, MAP_PRIVATE, fd, 0);
            if (address != null) {
                *bytes = (int)length;
            }
        }
        close(fd);
    }
    return address;
}

void mem_unmap(void* address, int bytes) {
    if (address != null) {
        munmap(address, bytes);
    }
}

#endif

END_C

#endif // IMPLEMENT_MEM

