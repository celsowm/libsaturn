#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

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
    extern char __bss_end;
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
