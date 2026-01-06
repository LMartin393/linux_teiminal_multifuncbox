// src/common/progress.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "progress.h"
#include "colors.h"

// 创建进度条
ProgressBar* progress_create(int width, const char *color) {
    ProgressBar *bar = malloc(sizeof(ProgressBar));
    if (bar == NULL) return NULL;
    
    bar->width = width > 0 ? width : 50;
    bar->filled = '=';
    bar->empty = '-';
    bar->color = color ? color : COLOR_BRIGHT_BLUE;
    bar->show_percent = 1;
    bar->show_text = 1;
    
    return bar;
}

// 销毁进度条
void progress_destroy(ProgressBar *bar) {
    if (bar != NULL) {
        free(bar);
    }
}

// 显示进度
void progress_show(ProgressBar *bar, float percentage, const char *text) {
    if (bar == NULL) return;
    
    // 限制百分比范围
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 1.0f) percentage = 1.0f;
    
    // 计算填充长度
    int filled = (int)(bar->width * percentage);
    int empty = bar->width - filled;
    
    // 打印进度条
    printf("\r[");
    
    // 填充部分
    if (bar->color) {
        printf("%s", bar->color);
    }
    for (int i = 0; i < filled; i++) {
        putchar(bar->filled);
    }
    if (bar->color) {
        printf("%s", COLOR_RESET);
    }
    
    // 空白部分
    for (int i = 0; i < empty; i++) {
        putchar(bar->empty);
    }
    printf("]");
    
    // 百分比
    if (bar->show_percent) {
        printf(" %5.1f%%", percentage * 100.0f);
    }
    
    // 文本
    if (bar->show_text && text != NULL) {
        printf(" %s", text);
    }
    
    // 清空该行剩余部分
    printf("          "); // 额外空格确保清除旧文本
    fflush(stdout);
}

// 完成进度条
void progress_finish(ProgressBar *bar, const char *message) {
    if (bar == NULL) return;
    
    // 显示100%
    progress_show(bar, 1.0f, message ? message : "完成");
    printf("\n");
}

// 简易进度函数（不需要创建进度条对象）
void simple_progress(const char *name, float percentage) {
    static const char *spinner = "|/-\\";
    static int spinner_idx = 0;
    
    if (percentage < 0) percentage = 0;
    if (percentage > 1) percentage = 1;
    
    int width = 20;
    int filled = (int)(width * percentage);
    
    printf("\r%s [", name);
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            printf("=");
        } else if (i == filled) {
            printf("%c", spinner[spinner_idx]);
            spinner_idx = (spinner_idx + 1) % 4;
        } else {
            printf(" ");
        }
    }
    printf("] %5.1f%%", percentage * 100.0f);
    fflush(stdout);
    
    if (percentage >= 1.0f) {
        printf("\n");
    }
}