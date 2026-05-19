#include "libc/stdio.h"

int _start(char *argv[], int argn) {
    const char *prompt = "$ ";
    char buf[64];

    putchar(' ');
    while(1) {
        print(prompt);
        gets(buf, 64);
        buf[strlen(buf)-1] = 0; // remove newline
        if(strlen(buf) == 4 && buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't') {
            break;
        } else {
            // split by spaces, and exec the first part as the command, and the rest as arguments
            char *cmd = buf;
            char *args[16];
            int arg_count = 0;
            for (int i = 0; buf[i]; i++) {
                if (buf[i] == ' ') {
                    buf[i] = 0; // null-terminate the command/argument
                    if (arg_count < 16) {
                        args[arg_count++] = &buf[i + 1];
                    }
                }
            }

            // if the command doesn't start with /, add a "/1/" prefix to look in the root filesystem
            if (cmd[0] != '/') {
                char temp[64];
                snprintf(temp, sizeof(temp), "/1/%s", cmd);
                cmd = temp;
            }

            int errno = sys_exec(cmd, args, arg_count);
            if (errno == -1) {
                print(cmd);
                puts(" is not an executable file or command.");
            }
            
            if (errno != 0) {
                putchar('x');
            } else {
                putchar(' ');
            }
        }
    }
    return 0;
}