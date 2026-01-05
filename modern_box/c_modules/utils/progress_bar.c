#include "progress_bar.h"
#include "../utils/ansi_unicode.h"
#include <stdio.h>
#include <string.h>

// 初始化进度条
void progress_bar_init(ProgressBar* bar, off_t total_size, const char* prefix) {
    bar->total = total_size;
    bar->current = 0;
    bar->prefix = prefix;
    bar->bar_width = 50; // 进度条宽度
    printf("\n"); // 换行预留进度条位置
}

// 更新进度条
void progress_bar_update(ProgressBar* bar, off_t current) {
    bar->current = current;
    double progress = (double)current / bar->total;
    int filled_width = (int)(progress * bar->bar_width);

    // ANSI光标移动：回到行首（实现动态刷新）
    printf("\r%s [", bar->prefix);

    // 填充进度条
    for (int i = 0; i < bar->bar_width; i++) {
        if (i < filled_width) {
            printf("█"); // Unicode块字符
        } else {
            printf(" ");
        }
    }

    // 显示百分比和字节数
    printf("] %.2f%% (%lld/%lld bytes)", 
           progress * 100, 
           (long long)current, 
           (long long)bar->total);
    fflush(stdout); // 强制刷新输出
}

// 完成进度条
void progress_bar_finish(ProgressBar* bar) {
    // 确保进度条填满
    progress_bar_update(bar, bar->total);
    printf("\n"); // 换行结束
}