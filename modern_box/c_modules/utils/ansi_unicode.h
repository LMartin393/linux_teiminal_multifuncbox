#ifndef ANSI_UNICODE_H
#define ANSI_UNICODE_H

#include <stdlib.h>
#include <stdint.h>

// ANSI转义序列定义
#define ANSI_RESET "\033[0m"
#define ANSI_COLOR_BLACK "\033[30m"
#define ANSI_COLOR_RED "\033[31m"
#define ANSI_COLOR_GREEN "\033[32m"
#define ANSI_COLOR_YELLOW "\033[33m"
#define ANSI_COLOR_BLUE "\033[34m"
#define ANSI_COLOR_MAGENTA "\033[35m"
#define ANSI_COLOR_CYAN "\033[36m"
#define ANSI_COLOR_WHITE "\033[37m"
#define ANSI_BOLD "\033[1m"

// 文件类型枚举
typedef enum {
    FILE_REGULAR,
    FILE_DIR,
    FILE_EXEC,
    FILE_LINK,
    FILE_SPECIAL,
    FILE_IMAGE,
    FILE_VIDEO,
    FILE_TEXT
} FileType;

// Git状态枚举
typedef enum {
    GIT_UNTRACKED,
    GIT_MODIFIED,
    GIT_ADDED,
    GIT_DELETED,
    GIT_COMMITTED
} GitStatus;

// 函数声明
const char* ansi_color(FileType type, GitStatus status);
void ansi_enable(int enable);
const char* unicode_icon(FileType type);
void print_unicode(const char* str);

#ifdef _WIN32
#include <windows.h>
#endif

#endif // ANSI_UNICODE_H