// src/file_tools/tkcpmv.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <utime.h>
#include "../common/colors.h"
#include "../common/utils.h"
#include "../common/progress.h"

#define BUFFER_SIZE 8192
#define MAX_PATH 4096

typedef struct {
    int verbose;
    int interactive;
    int force;
    int preserve;
    int recursive;
    int show_progress;
    int simulate;
    int follow_symlinks;
    char operation[10];  // "copy" 或 "move"
} Config;

typedef struct {
    long long total_size;
    long long copied_size;
    int total_files;
    int copied_files;
} ProgressInfo;

void print_help() {
    printf("tkcpmv - 智能复制/移动工具\n\n");
    printf("用法:\n");
    printf("  tkcpmv copy [选项] <源> <目标>\n");
    printf("  tkcpmv move [选项] <源> <目标>\n\n");
    
    printf("选项:\n");
    printf("  -v           详细输出\n");
    printf("  -i           交互式操作（覆盖前询问）\n");
    printf("  -f           强制覆盖\n");
    printf("  -p           保留文件属性（时间戳、权限）\n");
    printf("  -r/-R        递归复制目录\n");
    printf("  -P           显示进度条\n");
    printf("  -n           模拟运行（不实际操作）\n");
    printf("  -L           跟随符号链接\n");
    printf("  -s <大小>    设置缓冲区大小（KB）\n");
    printf("  -h           显示帮助\n\n");
    
    printf("示例:\n");
    printf("  tkcpmv copy -v file.txt backup/\n");
    printf("  tkcpmv move -ir old/ new/\n");
    printf("  tkcpmv copy -PR source_dir/ dest_dir/\n");
    printf("  tkcpmv copy -n file1 file2 dir/  # 模拟复制\n");
}

// 询问用户确认
int ask_user(const char *question) {
    printf("%s [y/N] ", question);
    fflush(stdout);
    
    char answer[10];
    if (fgets(answer, sizeof(answer), stdin)) {
        trim_string(answer);
        return (strcasecmp(answer, "y") == 0 || strcasecmp(answer, "yes") == 0);
    }
    return 0;
}

// 获取文件大小
long long get_file_size(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    
    if (S_ISLNK(st.st_mode)) {
        // 符号链接本身很小
        return 0;
    } else if (S_ISREG(st.st_mode)) {
        return st.st_size;
    } else if (S_ISDIR(st.st_mode)) {
        // 目录大小需要递归计算
        long long total = 4096;  // 目录本身的大小
        DIR *dir = opendir(path);
        if (!dir) return total;
        
        struct dirent *entry;
        char subpath[MAX_PATH];
        
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
            total += get_file_size(subpath);
        }
        
        closedir(dir);
        return total;
    }
    
    return 0;
}

// 复制单个文件
int copy_file(const char *src, const char *dst, Config *config, ProgressInfo *info, 
              ProgressBar *bar) {
    if (config->simulate) {
        if (config->verbose) {
            printf("模拟: 复制 %s -> %s\n", src, dst);
        }
        return 1;
    }
    
    // 检查源文件是否存在且可读
    if (access(src, R_OK) != 0) {
        print_error("无法读取文件: %s", src);
        return 0;
    }
    
    // 检查目标文件是否已存在
    if (access(dst, F_OK) == 0) {
        if (config->interactive) {
            if (!ask_user("覆盖文件？")) {
                print_info("跳过: %s", dst);
                return 1;
            }
        } else if (!config->force) {
            print_error("文件已存在: %s (使用 -f 强制覆盖)", dst);
            return 0;
        }
    }
    
    // 打开源文件
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        print_error("无法打开源文件: %s", src);
        return 0;
    }
    
    // 创建目标文件
    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        print_error("无法创建目标文件: %s", dst);
        close(src_fd);
        return 0;
    }
    
    // 复制数据
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    long long total_copied = 0;
    
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            print_error("写入失败: %s", dst);
            close(src_fd);
            close(dst_fd);
            unlink(dst);  // 删除不完整的文件
            return 0;
        }
        
        total_copied += bytes_written;
        
        // 更新进度条
        if (bar && info->total_size > 0) {
            info->copied_size += bytes_written;
            float progress = (float)info->copied_size / info->total_size;
            progress_show(bar, progress, "复制中...");
        }
    }
    
    if (bytes_read < 0) {
        print_error("读取失败: %s", src);
        close(src_fd);
        close(dst_fd);
        unlink(dst);
        return 0;
    }
    
    close(src_fd);
    close(dst_fd);
    
    // 保留文件属性
    if (config->preserve) {
        struct stat st;
        struct utimbuf times;
        
        if (stat(src, &st) == 0) {
            // 设置权限
            chmod(dst, st.st_mode & 0777);
            
            // 设置时间戳
            times.actime = st.st_atime;
            times.modtime = st.st_mtime;
            utime(dst, &times);
        }
    }
    
    if (config->verbose) {
        printf("复制: %s -> %s (%s)\n", src, dst, format_size(total_copied));
    }
    
    return 1;
}

// 移动文件（先复制后删除）
int move_file(const char *src, const char *dst, Config *config, ProgressInfo *info, 
              ProgressBar *bar) {
    if (config->simulate) {
        if (config->verbose) {
            printf("模拟: 移动 %s -> %s\n", src, dst);
        }
        return 1;
    }
    
    // 尝试直接重命名（最有效的移动方式）
    if (rename(src, dst) == 0) {
        if (config->verbose) {
            printf("移动: %s -> %s\n", src, dst);
        }
        return 1;
    }
    
    // 重命名失败（跨文件系统），先复制后删除
    if (config->verbose) {
        printf("跨文件系统移动，使用复制+删除方式\n");
    }
    
    if (!copy_file(src, dst, config, info, bar)) {
        return 0;
    }
    
    // 删除源文件
    if (unlink(src) != 0) {
        print_error("无法删除源文件: %s", src);
        // 尝试删除已复制的文件
        unlink(dst);
        return 0;
    }
    
    return 1;
}

// 复制目录
int copy_directory(const char *src, const char *dst, Config *config, ProgressInfo *info, 
                   ProgressBar *bar) {
    // 创建目标目录
    if (!config->simulate && mkdir(dst, 0755) != 0) {
        if (errno != EEXIST) {
            print_error("无法创建目录: %s", dst);
            return 0;
        }
    }
    
    DIR *dir = opendir(src);
    if (!dir) {
        print_error("无法打开目录: %s", src);
        return 0;
    }
    
    struct dirent *entry;
    char src_path[MAX_PATH], dst_path[MAX_PATH];
    int success = 1;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);
        
        struct stat st;
        if (lstat(src_path, &st) != 0) {
            print_error("无法获取文件信息: %s", src_path);
            success = 0;
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            if (config->recursive) {
                if (!copy_directory(src_path, dst_path, config, info, bar)) {
                    success = 0;
                }
            } else if (config->verbose) {
                print_info("跳过目录: %s (使用 -r 递归复制)", src_path);
            }
        } else if (S_ISREG(st.st_mode) || (S_ISLNK(st.st_mode) && config->follow_symlinks)) {
            if (strcmp(config->operation, "copy") == 0) {
                if (!copy_file(src_path, dst_path, config, info, bar)) {
                    success = 0;
                }
            } else {
                if (!move_file(src_path, dst_path, config, info, bar)) {
                    success = 0;
                }
            }
        } else if (S_ISLNK(st.st_mode)) {
            // 处理符号链接
            char link_target[MAX_PATH];
            ssize_t len = readlink(src_path, link_target, sizeof(link_target) - 1);
            if (len > 0) {
                link_target[len] = '\0';
                if (!config->simulate && symlink(link_target, dst_path) != 0) {
                    print_error("无法创建符号链接: %s", dst_path);
                    success = 0;
                } else if (config->verbose) {
                    printf("链接: %s -> %s\n", dst_path, link_target);
                }
            }
        }
    }
    
    closedir(dir);
    
    // 复制目录属性
    if (config->preserve && !config->simulate) {
        struct stat st;
        if (stat(src, &st) == 0) {
            chmod(dst, st.st_mode & 0777);
            // 注意：目录的时间戳可能无法完全保留
        }
    }
    
    return success;
}

// 计算要处理的文件总大小
long long calculate_total_size(char **sources, int num_sources, Config *config) {
    long long total = 0;
    
    for (int i = 0; i < num_sources; i++) {
        struct stat st;
        if (lstat(sources[i], &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode) && config->recursive) {
            total += get_file_size(sources[i]);
        } else if (S_ISREG(st.st_mode) || (S_ISLNK(st.st_mode) && config->follow_symlinks)) {
            total += st.st_size;
        }
    }
    
    return total;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }
    
    // 检查操作类型
    if (strcmp(argv[1], "copy") != 0 && strcmp(argv[2], "move") != 0) {
        print_error("第一个参数必须是 'copy' 或 'move'");
        print_help();
        return 1;
    }
    
    Config config = {
        .verbose = 0,
        .interactive = 0,
        .force = 0,
        .preserve = 0,
        .recursive = 0,
        .show_progress = 0,
        .simulate = 0,
        .follow_symlinks = 0
    };
    strcpy(config.operation, argv[1]);
    
    // 解析选项
    int opt;
    int buffer_size_kb = 8;  // 默认8KB
    
    while ((opt = getopt(argc, argv, "vifprRPnLs:h")) != -1) {
        switch (opt) {
            case 'v':
                config.verbose = 1;
                break;
            case 'i':
                config.interactive = 1;
                break;
            case 'f':
                config.force = 1;
                break;
            case 'p':
                config.preserve = 1;
                break;
            case 'r':
            case 'R':
                config.recursive = 1;
                break;
            case 'P':
                config.show_progress = 1;
                break;
            case 'n':
                config.simulate = 1;
                break;
            case 'L':
                config.follow_symlinks = 1;
                break;
            case 's':
                buffer_size_kb = atoi(optarg);
                if (buffer_size_kb < 1) buffer_size_kb = 8;
                break;
            case 'h':
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }
    
    // 检查剩余参数
    if (optind >= argc) {
        print_error("需要指定源文件");
        print_help();
        return 1;
    }
    
    int num_sources = argc - optind - 1;
    if (num_sources < 1) {
        print_error("需要指定目标位置");
        print_help();
        return 1;
    }
    
    char **sources = &argv[optind];
    char *destination = argv[argc - 1];
    
    // 检查目标是否为目录
    struct stat dst_stat;
    int dst_is_dir = 0;
    if (stat(destination, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode)) {
        dst_is_dir = 1;
    }
    
    // 如果多个源文件，目标必须是目录
    if (num_sources > 1 && !dst_is_dir && !config.simulate) {
        print_error("复制多个文件时，目标必须是目录");
        return 1;
    }
    
    // 计算总大小（用于进度条）
    ProgressInfo info = {0};
    info.total_size = calculate_total_size(sources, num_sources, &config);
    
    // 创建进度条
    ProgressBar *progress_bar = NULL;
    if (config.show_progress && info.total_size > 0) {
        progress_bar = progress_create(50, COLOR_BRIGHT_BLUE);
        printf("总大小: %s\n", format_size(info.total_size));
    }
    
    // 处理每个源文件
    int success = 1;
    for (int i = 0; i < num_sources; i++) {
        struct stat src_stat;
        if (lstat(sources[i], &src_stat) != 0) {
            print_error("源文件不存在: %s", sources[i]);
            success = 0;
            continue;
        }
        
        // 构建目标路径
        char dst_path[MAX_PATH];
        if (dst_is_dir) {
            // 从源路径提取文件名
            char *filename = strrchr(sources[i], '/');
            if (filename) {
                filename++;  // 跳过 '/'
            } else {
                filename = sources[i];
            }
            snprintf(dst_path, sizeof(dst_path), "%s/%s", destination, filename);
        } else {
            strcpy(dst_path, destination);
        }
        
        // 执行操作
        if (S_ISDIR(src_stat.st_mode)) {
            if (!config.recursive) {
                print_error("跳过目录: %s (使用 -r 递归复制)", sources[i]);
                success = 0;
                continue;
            }
            
            if (!copy_directory(sources[i], dst_path, &config, &info, progress_bar)) {
                success = 0;
            }
        } else if (S_ISREG(src_stat.st_mode) || (S_ISLNK(src_stat.st_mode) && config.follow_symlinks)) {
            if (strcmp(config.operation, "copy") == 0) {
                if (!copy_file(sources[i], dst_path, &config, &info, progress_bar)) {
                    success = 0;
                }
            } else {
                if (!move_file(sources[i], dst_path, &config, &info, progress_bar)) {
                    success = 0;
                }
            }
        } else if (S_ISLNK(src_stat.st_mode)) {
            // 处理符号链接
            char link_target[MAX_PATH];
            ssize_t len = readlink(sources[i], link_target, sizeof(link_target) - 1);
            if (len > 0) {
                link_target[len] = '\0';
                if (!config.simulate && symlink(link_target, dst_path) != 0) {
                    print_error("无法创建符号链接: %s", dst_path);
                    success = 0;
                } else if (config.verbose) {
                    printf("链接: %s -> %s\n", dst_path, link_target);
                }
            }
        } else {
            print_error("不支持的文件类型: %s", sources[i]);
            success = 0;
        }
    }
    
    // 完成进度条
    if (progress_bar) {
        progress_finish(progress_bar, success ? "完成" : "部分失败");
        progress_destroy(progress_bar);
    }
    
    // 显示统计信息
    if (config.verbose) {
        printf("\n操作统计:\n");
        printf("操作类型: %s\n", config.operation);
        printf("源文件数: %d\n", num_sources);
        printf("目标位置: %s\n", destination);
        printf("总数据量: %s\n", format_size(info.total_size));
        printf("操作状态: %s\n", success ? "成功" : "有错误");
    }
    
    return success ? 0 : 1;
}