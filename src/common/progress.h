// src/common/progress.h
#ifndef PROGRESS_H
#define PROGRESS_H

// 进度条结构
typedef struct {
    int width;           // 进度条宽度
    char filled;         // 填充字符
    char empty;          // 空白字符
    const char *color;   // 颜色
    int show_percent;    // 是否显示百分比
    int show_text;       // 是否显示文本
} ProgressBar;

// 创建和销毁
ProgressBar* progress_create(int width, const char *color);
void progress_destroy(ProgressBar *bar);

// 显示进度
void progress_show(ProgressBar *bar, float percentage, const char *text);
void progress_finish(ProgressBar *bar, const char *message);

// 简易进度函数
void simple_progress(const char *name, float percentage);

#endif // PROGRESS_H