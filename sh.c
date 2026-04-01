#include "libc/stdio.h"

int _start() {
    const char *prompt = "$ ";
    char buf[64];

    while(1) {
        print(prompt);
        gets(buf, 64);
        buf[strlen(buf)-1] = 0; // remove newline
        if(strlen(buf) == 4 && buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't') {
            break;
        } else {
            puts(buf);
        }
    }
    return 0;
}