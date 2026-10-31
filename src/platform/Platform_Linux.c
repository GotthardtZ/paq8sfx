// The libc-free stub on Linux (x64).
//
// The stub talks to the Linux kernel directly, with system calls. This file
// contains:
//   - the plat_* functions of Platform.h
//   - the C functions that only the Linux code of the stub calls
//     (stat, mkdir, chmod, execv)
//   - the entry point of the program

#include "Platform.h"

#if defined(SFX) && defined(SFX_FREESTANDING) && defined(__linux__)

#include <sys/types.h>
#include <sys/stat.h>

// ----------------------------------------------------------- system calls --
// A system call is made with the "syscall" instruction: its number goes in
// rax, its arguments in rdi, rsi, rdx, r10, r8 and r9, and the result comes
// back in rax. The kernel overwrites rcx and r11.

static inline long syscall3(long number, long a1, long a2, long a3) {
  long result;
  __asm__ volatile("syscall"
                   : "=a"(result)
                   : "a"(number), "D"(a1), "S"(a2), "d"(a3)
                   : "rcx", "r11", "memory");
  return result;
}

static inline long syscall6(long number, long a1, long a2, long a3, long a4, long a5, long a6) {
  long result;
  register long r10 __asm__("r10") = a4;
  register long r8  __asm__("r8")  = a5;
  register long r9  __asm__("r9")  = a6;
  __asm__ volatile("syscall"
                   : "=a"(result)
                   : "a"(number), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
                   : "rcx", "r11", "memory");
  return result;
}

// System call numbers (x64).
enum {
  SYS_read = 0, SYS_write = 1, SYS_open = 2, SYS_close = 3, SYS_stat = 4,
  SYS_lseek = 8, SYS_mmap = 9, SYS_munmap = 11, SYS_execve = 59,
  SYS_mkdir = 83, SYS_chmod = 90, SYS_exit_group = 231
};

static int lastError = 0;   // this is "errno"

// The kernel reports an error as a negative error number (-1 .. -4095).
// C functions return -1 instead and store the error number in errno.
static long orMinusOne(long result) {
  if (result < 0 && result >= -4095) {
    lastError = (int)-result;
    return -1;
  }
  return result;
}


// --------------------------------------------------------------- Platform.h

plat_file plat_open(const char* path, bool create) {
  // The values of O_RDONLY, O_RDWR, O_CREAT and O_TRUNC.
  const long READ_ONLY = 0, READ_WRITE = 02, CREATE = 0100, EMPTY = 01000;
  const long flags = create ? (READ_WRITE | CREATE | EMPTY) : READ_ONLY;
  return orMinusOne(syscall3(SYS_open, (long)path, flags, 0644));
}

void plat_close(plat_file file) {
  syscall3(SYS_close, file, 0, 0);
}

int64_t plat_read(plat_file file, void* buffer, size_t count) {
  return orMinusOne(syscall3(SYS_read, file, (long)buffer, (long)count));
}

int64_t plat_write(plat_file file, const void* buffer, size_t count) {
  return orMinusOne(syscall3(SYS_write, file, (long)buffer, (long)count));
}

int64_t plat_seek(plat_file file, int64_t offset, int whence) {
  return orMinusOne(syscall3(SYS_lseek, file, offset, whence));
}

void* plat_alloc(size_t size) {
  // The values of PROT_READ | PROT_WRITE and of MAP_PRIVATE | MAP_ANONYMOUS.
  const long READ_WRITE = 1 | 2, PRIVATE_MEMORY = 0x02 | 0x20;
  const long address = syscall6(SYS_mmap, 0, (long)size, READ_WRITE, PRIVATE_MEMORY, -1, 0);
  return orMinusOne(address) == -1 ? NULL : (void*)address;
}

void plat_free(void* memory, size_t size) {
  syscall3(SYS_munmap, (long)memory, (long)size, 0);
}

PLAT_NORETURN void plat_exit(int exitCode) {
  syscall3(SYS_exit_group, exitCode, 0, 0);
  __builtin_unreachable();
}

// ------------------------------------------------- C functions, Linux only --

// "errno" is a macro for *__errno_location() in the C library headers.
int* __errno_location(void) { return &lastError; }

// "struct stat" has the layout the kernel uses, so it is passed on as it is.
int stat(const char* path, struct stat* info) {
  return (int)orMinusOne(syscall3(SYS_stat, (long)path, (long)info, 0));
}

int mkdir(const char* path, mode_t mode) {
  return (int)orMinusOne(syscall3(SYS_mkdir, (long)path, mode, 0));
}

int chmod(const char* path, mode_t mode) {
  return (int)orMinusOne(syscall3(SYS_chmod, (long)path, mode, 0));
}

// The environment variables of this program; execv passes them on.
//
// environ is placed in .data on purpose. Without it the stub has no
// initialised data at all, its writable segment holds only .bss, and ld gives
// that segment a file offset inside the ELF headers. UPX (the usual way to
// ship the stub) cannot pack such a file: "EOFException: premature end of
// file". One 8-byte variable in .data avoids that:
__attribute__((section(".data.environ"))) char** environ = NULL;

int execv(const char* path, char* const argv[]) {
  return (int)orMinusOne(syscall3(SYS_execve, (long)path, (long)argv, (long)environ));
}

// ------------------------------------------------------------ entry point --
// The kernel starts the program at _start, with the stack holding:
//   argc, argv[0] .. argv[argc-1], null, envp[0] .. , null
// _start (below) passes the address of argc to startStub, which calls main
// and ends the program. (A C program has no constructors of global objects
// to run before main.)

int main(int argc, char** argv);

PLAT_NORETURN void startStub(long* stack);

PLAT_NORETURN void startStub(long* stack) {
  const int argc = (int)stack[0];
  char** argv = (char**)(stack + 1);
  environ = argv + argc + 1;
  plat_exit(main(argc, argv));
}

__asm__(
  ".text\n"
  ".global _start\n"
  "_start:\n"
  "  xor %rbp, %rbp\n"      // marks the outermost stack frame
  "  mov %rsp, %rdi\n"      // first argument of startStub: the address of argc
  "  and $-16, %rsp\n"      // a function must be called with a 16-byte aligned stack
  "  call startStub\n"
);

#endif
