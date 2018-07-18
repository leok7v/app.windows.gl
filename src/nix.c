#include "std.h"

BEGIN_C

static void* mem_map(const char* filename, int* bytes, bool readonly) {
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

void* mem_map_read(const char* filename, int* bytes) { return mem_map(filename, bytes, false); }

void* mem_map_read_write(const char* filename, int* bytes) { return mem_map(filename, bytes, true); }

void mem_unmap(void* address, int bytes) {
    if (address != null) {
        munmap(address, bytes);
    }
}

#endif

END_C


