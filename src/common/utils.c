// src/common/utils.c
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

// ========== 错误和信息输出 ==========
void print_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[错误] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void print_warning(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[警告] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void print_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[信息] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void print_success(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[成功] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

// ========== 文件检查 ==========
int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

int is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

int is_readable(const char *path) {
    return access(path, R_OK) == 0;
}

// ========== 格式化 ==========
char* format_size(long long bytes) {
    static char buffer[32];
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = (double)bytes;
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    if (unit == 0) {
        snprintf(buffer, sizeof(buffer), "%lld B", bytes);
    } else {
        snprintf(buffer, sizeof(buffer), "%.1f %s", size, units[unit]);
    }
    
    return buffer;
}

char* format_time(time_t timestamp) {
    static char buffer[64];
    struct tm *tm_info = localtime(&timestamp);
    
    // 简单格式：月-日 时:分
    strftime(buffer, sizeof(buffer), "%m-%d %H:%M", tm_info);
    return buffer;
}

// ========== 字符串处理 ==========
void trim_string(char *str) {
    if (str == NULL) return;
    
    // 去除开头空格
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    
    // 移动字符串
    if (start != str) {
        char *dest = str;
        while (*start) {
            *dest++ = *start++;
        }
        *dest = '\0';
    }
    
    // 去除结尾空格
    char *end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
}

int starts_with(const char *str, const char *prefix) {
    if (str == NULL || prefix == NULL) return 0;
    while (*prefix) {
        if (*prefix++ != *str++) return 0;
    }
    return 1;
}