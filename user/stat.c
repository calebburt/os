#include "libc/stdio.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Usage: stat <path>");
        return 1;
    }

    struct stat st;
    if (sys_stat(argv[1], &st) < 0) {
        print("stat: cannot stat '");
        print(argv[1]);
        puts("'");
        return 1;
    }

    char line[128];
    snprintf(line, sizeof(line), "  File: %s\n", argv[1]);
    print(line);
    snprintf(line, sizeof(line), "  Size: %d  Type: %s\n",
             (int)st.size, S_ISDIR(st.mode) ? "directory" : "regular");
    print(line);
    snprintf(line, sizeof(line), "  Mode: %d\n", (int)(st.mode & 0777));
    print(line);
    return 0;
}
