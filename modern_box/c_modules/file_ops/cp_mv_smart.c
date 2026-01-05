#include "cp_mv_smart.h"
#include "../utils/progress_bar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE (1024 * 1024)  // 1MB缓冲区

// 获取文件大小
off_t get_file_size(const char* file_path) {
    struct stat st;
    if (stat(file_path, &st) < 0) {
        perror("stat failed");
        return -1;
    }
    return st.st_size;
}

// 断点续传复制文件
int smart_cp(const char* src_path, const char* dst_path) {
    FILE *src_fp, *dst_fp;
    char buffer[BUFFER_SIZE];
    size_t read_size, write_size;
    off_t src_size, dst_size, offset = 0;
    ProgressBar bar;

    // 打开源文件
    src_fp = fopen(src_path, "rb");
    if (!src_fp) {
        perror("fopen src failed");
        return -1;
    }

    // 获取源文件大小
    src_size = get_file_size(src_path);
    if (src_size < 0) {
        fclose(src_fp);
        return -1;
    }

    // 检查目标文件是否存在（断点续传）
    if (access(dst_path, F_OK) == 0) {
        dst_size = get_file_size(dst_path);
        if (dst_size < src_size) {
            // 目标文件存在且未完成，从末尾继续写入
            dst_fp = fopen(dst_path, "ab");
            fseek(src_fp, dst_size, SEEK_SET);
            offset = dst_size;
            printf("Resuming copy from offset: %lld bytes\n", (long long)offset);
        } else {
            printf("Destination file already exists and is complete\n");
            fclose(src_fp);
            return 0;
        }
    } else {
        // 目标文件不存在，新建文件
        dst_fp = fopen(dst_path, "wb");
    }

    if (!dst_fp) {
        perror("fopen dst failed");
        fclose(src_fp);
        return -1;
    }

    // 初始化进度条
    progress_bar_init(&bar, src_size, "Copying: ");

    // 循环读写文件
    while ((read_size = fread(buffer, 1, BUFFER_SIZE, src_fp)) > 0) {
        write_size = fwrite(buffer, 1, read_size, dst_fp);
        if (write_size != read_size) {
            perror("fwrite failed");
            fclose(src_fp);
            fclose(dst_fp);
            return -1;
        }

        // 更新进度
        offset += write_size;
        progress_bar_update(&bar, offset);
        fflush(dst_fp); // 强制刷新缓冲区
    }

    // 完成进度条
    progress_bar_finish(&bar);

    // 关闭文件
    fclose(src_fp);
    fclose(dst_fp);
    printf("Copy completed successfully\n");
    return 0;
}

// 智能移动文件（优先重命名，跨分区则复制+删除）
int smart_mv(const char* src_path, const char* dst_path) {
    // 先尝试重命名（快速）
    if (rename(src_path, dst_path) == 0) {
        printf("Moved successfully (rename)\n");
        return 0;
    }

    // 重命名失败，采用复制+删除模式
    printf("Rename failed, using copy+delete mode\n");
    if (smart_cp(src_path, dst_path) != 0) {
        printf("Copy failed, move aborted\n");
        return -1;
    }

    // 删除源文件
    if (remove(src_path) != 0) {
        perror("remove src failed");
        printf("Copy completed but source file deletion failed\n");
        return -1;
    }

    printf("Moved successfully (copy+delete)\n");
    return 0;
}