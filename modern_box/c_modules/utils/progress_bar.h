#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H

#include <stdint.h>
#include <sys/types.h>

// 进度条结构体
typedef struct {
    off_t total;        // 总大小
    off_t current;      // 当前进度
    const char* prefix; // 前缀文本
    int bar_width;      // 进度条宽度
} ProgressBar;

// 函数声明
void progress_bar_init(ProgressBar* bar, off_t total_size, const char* prefix);
void progress_bar_update(ProgressBar* bar, off_t current);
void progress_bar_finish(ProgressBar* bar);

#endif // PROGRESS_BAR_H