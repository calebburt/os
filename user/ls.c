#include "libc/stdio.h"

int _start(char *argv[], int argn) {
    const char *path = argn > 0 ? argv[0] : "/1";

    long fd = sys_open((char*)path, O_RDONLY);
    if (fd < 0) {
        print("ls: cannot open '");
        print(path);
        puts("'");
        return 1;
    }

    struct dirent de;
    for (int i = 0; sys_readdir(fd, i, &de) == 0; i++) {
        if (S_ISDIR(de.mode)) {
            print(de.name);
            puts("/");
        } else {
            puts(de.name);
        }
    }

    sys_close(fd);
    return 0;
}
