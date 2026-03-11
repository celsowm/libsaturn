#include <stdint.h>
#include <stddef.h>

/* Minimal definitions to avoid pulling in sys/stat.h and sys/types.h
   which may not exist in bare-metal toolchains. */
typedef int32_t off_t;

int errno;
#define EINVAL 22

#define S_IFCHR 0020000

struct stat {
    unsigned short st_mode;
};

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *memset(void *dest, int c, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    while (n--) {
        *d++ = (unsigned char)c;
    }
    return dest;
}

void _exit(int status) {
    (void)status;
    for (;;) {
    }
}

int _close(int file) {
    (void)file;
    return -1;
}

int _fstat(int file, struct stat* st) {
    (void)file;
    if (st == 0) {
        errno = EINVAL;
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    (void)file;
    return 1;
}

off_t _lseek(int file, off_t ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char* ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

void* _sbrk(ptrdiff_t incr) {
    extern char __bss_end __asm__("__bss_end");
    static char* heap_end;
    char* prev_heap_end;
    if (heap_end == 0) {
        heap_end = &__bss_end;
    }
    prev_heap_end = heap_end;
    heap_end += incr;
    return prev_heap_end;
}

int _write(int file, const char* ptr, int len) {
    (void)file;
    (void)ptr;
    return len;
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void) {
    return 1;
}
