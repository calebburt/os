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
        // const char *buf = (const char *)frame->rdi;
        // uint64_t len = frame->rsi;
        // for (uint64_t i = 0; i < len; i++)
        //     putchar(buf[i]);
        // frame->rax = len;

        putchar((char)frame->rdi);
        frame->rax = 0;
        break;
    }
    case SYS_READ: {
        // char *buf = (char *)frame->rdi;
        // uint64_t len = frame->rsi;
        // for (uint64_t i = 0; i < len; i++)
        //     buf[i] = getchar();
        // frame->rax = len;

        frame->rax = getchar();
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
    case SYS_READ_FILE: {
        long handle = frame->rdi;
        struct open_file *file = NULL;
        for(int i = 0;i<MAX_OPENS;i++) {
            if (handles[i] != NULL && handles[i]->handle == handle) {
                file = handles[i];
                break;
            }
        }
        
        if (file != NULL) {
            char *buf = (char*)malloc(file->inode->size);

            vfs_read(file->inode, (uint8_t*)buf, file->inode->size);

            frame->rax = (long)buf;
        } else {
            frame->rax = (long)NULL;
        }
        break;
    }
    case SYS_WRITE_FILE: {
        long handle = frame->rdi;
        struct open_file *file = NULL;
        for(int i = 0;i<MAX_OPENS;i++) {
            if (handles[i] != NULL && handles[i]->handle == handle) {
                file = handles[i];
                break;
            }
        }
        
        if (file != NULL) {
            
        } else {
            frame->rax = -1;
        }
        break;
    }
    case SYS_EXEC: {
        char *path = (char*)frame->rdi;
        struct inode *file = vfs_open(path, O_RDONLY);

        if (file == NULL) {
            frame->rax = -1;
            break;
        }

        frame->rax = run(*file, frame->rsi, frame->rdx);
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
