#include "syscall.h"
#include "stdio.h"
#include "io.h"
#include "vfs.h"
#include "elf.h"
#include "mem.h"

// Syscall convention (matches Linux-style):
//   RAX = syscall number
//   RDI = arg1, RSI = arg2, RDX = arg3
//   Return value in RAX

static void syscall_handler(struct interrupt_frame *frame) {
    switch (frame->rax) {
    case SYS_EXIT: {
        shutdown();
        break;
    }
    case SYS_WRITE: {
        // RDI = file descriptor, RSI = buffer, RDX = length

        switch (frame->rdi) { // file descriptor (0 = stdin, 1 = stdout, 2 = stderr)
            case 0: // stdin
                frame->rax = -1; // cannot write to stdin
                break;
            case 1: // stdout
                // fallthrough for now
            case 2: // stderr
                for (uint64_t i = 0; i < frame->rdx; i++)
                    putchar(((char*)frame->rsi)[i]);
                frame->rax = frame->rdx;
                break;
            default:
                long handle = frame->rdi;
                struct open_file *file = NULL;
                for(int i = 0;i<MAX_OPENS;i++) {
                    if (handles[i] != NULL && handles[i]->handle == handle) {
                        file = handles[i];
                        break;
                    }
                }
                
                if (file != NULL) {
                    frame->rax = vfs_write(file->inode, (uint8_t*)frame->rsi, frame->rdx);
                } else {
                    frame->rax = -1;
                }
                break;
        }
        break;
    }
    case SYS_READ: {
        // RDI = file descriptor, RSI = buffer, RDX = length
        switch (frame->rdi) { // file descriptor (0 = stdin, 1 = stdout, 2 = stderr)
            case 0: { // stdin — line-buffered: stop after '\n'
                char *buf = (char*)frame->rsi;
                uint64_t i;
                for (i = 0; i < frame->rdx; i++) {
                    buf[i] = getchar();
                    if (buf[i] == '\n') { i++; break; }
                }
                frame->rax = i;
                break;
            }
            case 1: // stdout
            case 2: // stderr
                frame->rax = -1; // cannot read from stdout/stderr
                break;
            default:
                long handle = frame->rdi;
                struct open_file *file = NULL;
                for(int i = 0;i<MAX_OPENS;i++) {
                    if (handles[i] != NULL && handles[i]->handle == handle) {
                        file = handles[i];
                        break;
                    }
                }
                
                if (file != NULL) {
                    frame->rax = vfs_read(file->inode, (uint8_t*)frame->rsi, frame->rdx);
                } else {
                    frame->rax = -1;
                }
                break;
        }
        break;
    }
    case SYS_OPEN: {
        char *path = (char*)frame->rdi;
        long flags = frame->rsi;

        struct open_file *file = vfs_open_handle(path, flags);
        if (!file) {
            frame->rax = -1;
            break;
        }

        frame->rax = (long)file->handle;
        break;
    }
    case SYS_CLOSE: {
        long handle = frame->rdi;
        int found = 0;
        for(int i = 0;i<MAX_OPENS;i++) {
            if (handles[i] != NULL && handles[i]->handle == handle) {
                free(handles[i]);
                handles[i] = NULL;
                found = 1;
                break;
            }
        }
        frame->rax = found ? 0 : -1;
        break;
    }
    case SYS_UNLINK: {
        char *path = (char*)frame->rdi;
        frame->rax = vfs_unlink(path);
        break;
    }
    case SYS_LSEEK: {
        long handle = frame->rdi;
        long offset = frame->rsi;
        int whence = frame->rdx;
        struct open_file *file = NULL;
        for(int i = 0;i<MAX_OPENS;i++) {
            if (handles[i] != NULL && handles[i]->handle == handle) {
                file = handles[i];
                break;
            }
        }
        if (file == NULL || file->inode == NULL) {
            frame->rax = -1;
            break;
        }
        if (vfs_seek(file->inode, offset, whence) < 0) {
            frame->rax = -1;
            break;
        }
        frame->rax = file->inode->position;
        break;
    }
    case SYS_EXEC: {
        char *path = (char*)frame->rdi;
        struct inode *file = vfs_open(path, O_RDONLY);

        if (file == NULL) {
            frame->rax = -1;
            break;
        }

        frame->rax = run(*file, (char**)frame->rsi, frame->rdx);
        break;
    }
    default:
        frame->rax = (uint64_t)-1;
        break;
    }
}

void syscall_init(void) {
    isr_register_handler(0x80, syscall_handler);
}
