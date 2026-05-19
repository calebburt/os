#include "libc/stdio.h"

int _start(char *argv[], int argn) {
    if (argn < 1) {
        puts("Usage: stat <path>");
        return 1;
    }

    struct stat st;
    if (sys_stat(argv[0], &st) < 0) {
        print("stat: cannot stat '");
        print(argv[0]);
        puts("'");
        return 1;
    }

    char line[128];
    snprintf(line, sizeof(line), "  File: %s\n", argv[0]);
    print(line);
    snprintf(line, sizeof(line), "  Size: %d  Type: %s\n",
             (int)st.size, S_ISDIR(st.mode) ? "directory" : "regular");
    print(line);
    snprintf(line, sizeof(line), "  Mode: %d\n", (int)(st.mode & 0777));
    print(line);
    return 0;
}
