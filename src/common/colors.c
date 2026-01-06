// src/common/colors.c
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include "colors.h"

// 全局颜色启用状态
static int color_enabled = 1;

// 检查终端是否支持颜色
int is_color_supported(void) {
    // 检查是否在终端中且支持颜色
    if (!isatty(STDOUT_FILENO)) {
        return 0;
    }
    
    // 检查TERM环境变量
    char *term = getenv("TERM");
    if (term == NULL) {
        return 0;
    }
    
    // 检查是否是支持颜色的终端
    const char *color_terms[] = {
        "xterm", "xterm-256color", "xterm-color",
        "screen", "screen-256color", "linux",
        "vt100", "vt220", "ansi", "rxvt", "rxvt-unicode",
        NULL
    };
    
    for (int i = 0; color_terms[i] != NULL; i++) {
        if (strstr(term, color_terms[i]) != NULL) {
            return 1;
        }
    }
    
    return 0;
}

void enable_color(void) {
    color_enabled = 1;
}

void disable_color(void) {
    color_enabled = 0;
}

// 打印带颜色的文本（不带换行）
void color_print(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    if (color_enabled && color != NULL) {
        printf("%s", color);
    }
    
    vprintf(format, args);
    
    if (color_enabled && color != NULL) {
        printf("%s", COLOR_RESET);
    }
    
    va_end(args);
}

// 打印带颜色的文本（带换行）
void color_println(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    if (color_enabled && color != NULL) {
        printf("%s", color);
    }
    
    vprintf(format, args);
    
    if (color_enabled && color != NULL) {
        printf("%s", COLOR_RESET);
    }
    
    printf("\n");
    va_end(args);
}