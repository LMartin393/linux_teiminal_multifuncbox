#include "config.h"
#include "c_modules/file_ops/ls_enhanced.h"
#include "c_modules/file_ops/cp_mv_smart.h"
#include "c_modules/utils/ansi_unicode.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ModernBox - Modern Command Line Toolset\n");
        printf("Usage:\n");
        printf("  modernbox ls [dir]          - Enhanced list directory\n");
        printf("  modernbox cp <src> <dst>    - Smart copy with progress\n");
        printf("  modernbox mv <src> <dst>    - Smart move with resume\n");
        return 1;
    }

    // 解析命令
    if (strcmp(argv[1], "ls") == 0) {
        const char* dir = argc > 2 ? argv[2] : ".";
        ls_enhanced(dir);
    } else if (strcmp(argv[1], "cp") == 0) {
        if (argc < 4) {
            printf("Missing src/dst for cp command\n");
            return 1;
        }
        smart_cp(argv[2], argv[3]);
    } else if (strcmp(argv[1], "mv") == 0) {
        if (argc < 4) {
            printf("Missing src/dst for mv command\n");
            return 1;
        }
        smart_mv(argv[2], argv[3]);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}