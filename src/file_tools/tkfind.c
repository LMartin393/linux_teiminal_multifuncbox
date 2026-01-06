// src/file_tools/tkfind.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <fnmatch.h>
#include <regex.h>
#include <errno.h>
#include <ctype.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 搜索选项
typedef struct {
    char *name_pattern;     // 文件名模式
    char *content_pattern;  // 内容模式
    char *type_filter;      // 文件类型过滤
    char *size_filter;      // 大小过滤
    char *time_filter;      // 时间过滤
    int regex_mode;         // 使用正则表达式
    int ignore_case;        // 忽略大小写
    int recursive;          // 递归搜索
    int print_path;         // 打印完整路径
    int color_output;       // 彩色输出
    int show_stats;         // 显示统计信息
    int show_details;       // 显示详细信息
    int help;               // 帮助
    int version;            // 版本
    char **paths;           // 搜索路径
    int path_count;         // 路径数量
} Options;

// 搜索结果
typedef struct {
    char *path;             // 文件路径
    struct stat info;       // 文件信息
    int matches;            // 匹配行数
} SearchResult;

// 初始化选项
static void init_options(Options *opts) {
    opts->name_pattern = NULL;
    opts->content_pattern = NULL;
    opts->type_filter = NULL;
    opts->size_filter = NULL;
    opts->time_filter = NULL;
    opts->regex_mode = 0;
    opts->ignore_case = 0;
    opts->recursive = 1;    // 默认递归
    opts->print_path = 1;
    opts->color_output = is_color_supported();
    opts->show_stats = 0;
    opts->show_details = 0;
    opts->help = 0;
    opts->version = 0;
    opts->paths = NULL;
    opts->path_count = 0;
}

// 显示帮助
static void show_help() {
    printf("tkfind - 增强版文件查找工具\n");
    printf("用法: tkfind [路径] [选项]\n");
    printf("选项:\n");
    printf("  -name PATTERN      按文件名匹配\n");
    printf("  -iname PATTERN     按文件名匹配（忽略大小写）\n");
    printf("  -regex PATTERN     使用正则表达式匹配文件名\n");
    printf("  -iregex PATTERN    使用正则表达式匹配文件名（忽略大小写）\n");
    printf("  -type TYPE         按文件类型过滤\n");
    printf("                     f: 普通文件, d: 目录, l: 符号链接\n");
    printf("  -size SIZE         按文件大小过滤\n");
    printf("                     +100k: 大于100KB, -1M: 小于1MB\n");
    printf("  -mtime DAYS        按修改时间过滤\n");
    printf("                     +7: 超过7天, -1: 1天内\n");
    printf("  -content PATTERN   搜索文件内容\n");
    printf("  -i, --ignore-case  忽略大小写（内容搜索）\n");
    printf("  -r, --recursive    递归搜索子目录（默认）\n");
    printf("  -maxdepth LEVEL    最大搜索深度\n");
    printf("  -print             打印完整路径\n");
    printf("  -ls                类似ls -l的格式显示\n");
    printf("  -stat              显示统计信息\n");
    printf("      --no-color     无颜色输出\n");
    printf("      --help         显示帮助\n");
    printf("      --version      显示版本\n");
}

// 显示版本
static void show_version() {
    printf("tkfind v1.0.0 - TermKit 文件查找工具\n");
}

// 解析大小过滤器
static int parse_size_filter(const char *filter, long long *size) {
    if (!filter || !*filter) return 0;
    
    char *endptr;
    long long value = strtoll(filter, &endptr, 10);
    
    if (endptr == filter) return 0;
    
    // 处理单位
    switch (*endptr) {
        case 'k':
        case 'K':
            value *= 1024;
            break;
        case 'm':
        case 'M':
            value *= 1024 * 1024;
            break;
        case 'g':
        case 'G':
            value *= 1024 * 1024 * 1024;
            break;
        case '\0':
            break;
        default:
            return 0;
    }
    
    *size = value;
    return 1;
}

// 解析选项
static int parse_options(int argc, char **argv, Options *opts) {
    int max_depth = -1;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-name") == 0) {
            if (i + 1 < argc) {
                opts->name_pattern = argv[++i];
            }
        } else if (strcmp(argv[i], "-iname") == 0) {
            if (i + 1 < argc) {
                opts->name_pattern = argv[++i];
                opts->ignore_case = 1;
            }
        } else if (strcmp(argv[i], "-regex") == 0) {
            if (i + 1 < argc) {
                opts->name_pattern = argv[++i];
                opts->regex_mode = 1;
            }
        } else if (strcmp(argv[i], "-iregex") == 0) {
            if (i + 1 < argc) {
                opts->name_pattern = argv[++i];
                opts->regex_mode = 1;
                opts->ignore_case = 1;
            }
        } else if (strcmp(argv[i], "-type") == 0) {
            if (i + 1 < argc) {
                opts->type_filter = argv[++i];
            }
        } else if (strcmp(argv[i], "-size") == 0) {
            if (i + 1 < argc) {
                opts->size_filter = argv[++i];
            }
        } else if (strcmp(argv[i], "-mtime") == 0) {
            if (i + 1 < argc) {
                opts->time_filter = argv[++i];
            }
        } else if (strcmp(argv[i], "-content") == 0) {
            if (i + 1 < argc) {
                opts->content_pattern = argv[++i];
            }
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-case") == 0) {
            opts->ignore_case = 1;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
            opts->recursive = 1;
        } else if (strcmp(argv[i], "-maxdepth") == 0) {
            if (i + 1 < argc) {
                max_depth = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-print") == 0) {
            opts->print_path = 1;
        } else if (strcmp(argv[i], "-ls") == 0) {
            opts->show_details = 1;
        } else if (strcmp(argv[i], "-stat") == 0) {
            opts->show_stats = 1;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            opts->color_output = 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            opts->help = 1;
            return 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            opts->version = 1;
            return 1;
        } else if (argv[i][0] == '-') {
            print_error("无效选项: %s", argv[i]);
            return -1;
        } else {
            // 路径参数
            opts->path_count++;
            opts->paths = realloc(opts->paths, sizeof(char*) * opts->path_count);
            opts->paths[opts->path_count - 1] = argv[i];
        }
    }
    
    // 如果没有指定路径，使用当前目录
    if (opts->path_count == 0) {
        opts->path_count = 1;
        opts->paths = malloc(sizeof(char*));
        opts->paths[0] = ".";
    }
    
    return 1;
}

// 检查文件名是否匹配模式
static int match_name(const char *filename, const char *pattern, 
                     int regex_mode, int ignore_case) {
    if (!pattern) return 1;  // 没有模式，匹配所有
    
    if (regex_mode) {
        regex_t regex;
        int flags = REG_EXTENDED;
        if (ignore_case) flags |= REG_ICASE;
        
        if (regcomp(&regex, pattern, flags) != 0) {
            return 0;
        }
        
        int result = regexec(&regex, filename, 0, NULL, 0);
        regfree(&regex);
        
        return result == 0;
    } else {
        // 使用fnmatch进行通配符匹配
        int flags = 0;
        if (ignore_case) flags |= FNM_CASEFOLD;
        
        return fnmatch(pattern, filename, flags) == 0;
    }
}

// 检查文件类型是否匹配
static int match_type(const char *type_filter, mode_t mode) {
    if (!type_filter) return 1;
    
    switch (type_filter[0]) {
        case 'f':
            return S_ISREG(mode);
        case 'd':
            return S_ISDIR(mode);
        case 'l':
            return S_ISLNK(mode);
        default:
            return 0;
    }
}

// 检查文件大小是否匹配
static int match_size(const char *size_filter, off_t size) {
    if (!size_filter) return 1;
    
    char *filter = strdup(size_filter);
    char *ptr = filter;
    int greater_than = 0;
    int less_than = 0;
    
    // 解析比较符
    if (*ptr == '+') {
        greater_than = 1;
        ptr++;
    } else if (*ptr == '-') {
        less_than = 1;
        ptr++;
    }
    
    long long filter_size;
    if (!parse_size_filter(ptr, &filter_size)) {
        free(filter);
        return 0;
    }
    
    free(filter);
    
    if (greater_than) {
        return size > filter_size;
    } else if (less_than) {
        return size < filter_size;
    } else {
        return size == filter_size;
    }
}

// 检查文件时间是否匹配
static int match_time(const char *time_filter, time_t mtime) {
    if (!time_filter) return 1;
    
    char *filter = strdup(time_filter);
    char *ptr = filter;
    int greater_than = 0;
    int less_than = 0;
    
    // 解析比较符
    if (*ptr == '+') {
        greater_than = 1;
        ptr++;
    } else if (*ptr == '-') {
        less_than = 1;
        ptr++;
    }
    
    long days = atol(ptr);
    time_t now = time(NULL);
    time_t diff = now - mtime;
    time_t day_seconds = days * 24 * 3600;
    
    free(filter);
    
    if (greater_than) {
        return diff > day_seconds;
    } else if (less_than) {
        return diff < day_seconds;
    } else {
        return diff <= day_seconds && diff >= (day_seconds - 86400);
    }
}

// 在文件中搜索内容
static int search_content(const char *path, const char *pattern, 
                         int ignore_case, int *match_count) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    
    char line[4096];
    int line_num = 0;
    *match_count = 0;
    
    // 准备正则表达式
    regex_t regex;
    int flags = REG_EXTENDED;
    if (ignore_case) flags |= REG_ICASE;
    
    if (regcomp(&regex, pattern, flags) != 0) {
        fclose(file);
        return 0;
    }
    
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        
        // 去除换行符
        line[strcspn(line, "\n")] = '\0';
        
        if (regexec(&regex, line, 0, NULL, 0) == 0) {
            (*match_count)++;
        }
    }
    
    regfree(&regex);
    fclose(file);
    
    return 1;
}

// 获取文件颜色
static const char* get_file_color(mode_t mode) {
    if (S_ISDIR(mode)) return COLOR_BRIGHT_BLUE;
    if (S_ISLNK(mode)) return COLOR_BRIGHT_CYAN;
    if (mode & S_IXUSR) return COLOR_BRIGHT_GREEN;
    return COLOR_WHITE;
}

// 递归搜索目录
static void search_directory(const char *path, Options *opts, 
                            int depth, int max_depth,
                            SearchResult **results, int *result_count,
                            int *file_count, int *dir_count) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (lstat(full_path, &st) == -1) {
            continue;
        }
        
        // 检查文件名匹配
        if (!match_name(entry->d_name, opts->name_pattern, 
                       opts->regex_mode, opts->ignore_case)) {
            goto check_recursive;
        }
        
        // 检查文件类型
        if (!match_type(opts->type_filter, st.st_mode)) {
            goto check_recursive;
        }
        
        // 检查文件大小
        if (!match_size(opts->size_filter, st.st_size)) {
            goto check_recursive;
        }
        
        // 检查修改时间
        if (!match_time(opts->time_filter, st.st_mtime)) {
            goto check_recursive;
        }
        
        // 如果是文件，检查内容
        if (S_ISREG(st.st_mode) && opts->content_pattern) {
            int match_count = 0;
            if (!search_content(full_path, opts->content_pattern, 
                              opts->ignore_case, &match_count) || match_count == 0) {
                goto check_recursive;
            }
        }
        
        // 添加到结果
        (*result_count)++;
        *results = realloc(*results, sizeof(SearchResult) * (*result_count));
        SearchResult *result = &(*results)[*result_count - 1];
        
        result->path = strdup(full_path);
        result->info = st;
        result->matches = 0;
        
        if (S_ISDIR(st.st_mode)) {
            (*dir_count)++;
        } else {
            (*file_count)++;
        }
        
    check_recursive:
        // 递归搜索子目录
        if (S_ISDIR(st.st_mode) && opts->recursive && 
            (max_depth == -1 || depth < max_depth)) {
            search_directory(full_path, opts, depth + 1, max_depth,
                           results, result_count, file_count, dir_count);
        }
    }
    
    closedir(dir);
}

// 显示文件信息（类似ls -l）
static void show_file_details(const char *path, struct stat *st, Options *opts) {
    // 权限
    char perm[11];
    perm[0] = S_ISDIR(st->st_mode) ? 'd' : 
              S_ISLNK(st->st_mode) ? 'l' : 
              S_ISREG(st->st_mode) ? '-' : '?';
    
    perm[1] = (st->st_mode & S_IRUSR) ? 'r' : '-';
    perm[2] = (st->st_mode & S_IWUSR) ? 'w' : '-';
    perm[3] = (st->st_mode & S_IXUSR) ? 'x' : '-';
    perm[4] = (st->st_mode & S_IRGRP) ? 'r' : '-';
    perm[5] = (st->st_mode & S_IWGRP) ? 'w' : '-';
    perm[6] = (st->st_mode & S_IXGRP) ? 'x' : '-';
    perm[7] = (st->st_mode & S_IROTH) ? 'r' : '-';
    perm[8] = (st->st_mode & S_IWOTH) ? 'w' : '-';
    perm[9] = (st->st_mode & S_IXOTH) ? 'x' : '-';
    perm[10] = '\0';
    
    // 时间
    char *time_str = format_time(st->st_mtime);
    
    // 大小
    char *size_str = format_size(st->st_size);
    
    if (opts->color_output) {
        const char *color = get_file_color(st->st_mode);
        
        printf("%s %3ld %8s %s", perm, (long)st->st_nlink, size_str, time_str);
        printf(" %s%s%s", color, path, COLOR_RESET);
        
        // 如果是符号链接，显示目标
        if (S_ISLNK(st->st_mode)) {
            char target[4096];
            ssize_t len = readlink(path, target, sizeof(target) - 1);
            if (len != -1) {
                target[len] = '\0';
                printf(" -> %s", target);
            }
        }
    } else {
        printf("%s %3ld %8s %s %s", perm, (long)st->st_nlink, size_str, time_str, path);
        
        if (S_ISLNK(st->st_mode)) {
            char target[4096];
            ssize_t len = readlink(path, target, sizeof(target) - 1);
            if (len != -1) {
                target[len] = '\0';
                printf(" -> %s", target);
            }
        }
    }
    
    printf("\n");
}

// 显示搜索结果
static void show_results(SearchResult *results, int result_count, 
                        Options *opts, int file_count, int dir_count) {
    // 显示统计信息
    if (opts->show_stats) {
        if (opts->color_output) {
            color_print(COLOR_BRIGHT_CYAN, "找到 ");
            color_print(COLOR_BRIGHT_GREEN, "%d", result_count);
            color_print(COLOR_BRIGHT_CYAN, " 个项目");
            if (file_count > 0) {
                printf(" (");
                color_print(COLOR_BRIGHT_GREEN, "%d", file_count);
                printf(" 个文件");
            }
            if (dir_count > 0) {
                printf(", ");
                color_print(COLOR_BRIGHT_BLUE, "%d", dir_count);
                printf(" 个目录");
            }
            printf(")\n\n");
        } else {
            printf("找到 %d 个项目", result_count);
            if (file_count > 0) {
                printf(" (%d 个文件", file_count);
                if (dir_count > 0) {
                    printf(", %d 个目录", dir_count);
                }
                printf(")");
            }
            printf("\n\n");
        }
    }
    
    // 显示结果
    for (int i = 0; i < result_count; i++) {
        SearchResult *result = &results[i];
        
        if (opts->show_details) {
            show_file_details(result->path, &result->info, opts);
        } else {
            if (opts->color_output) {
                const char *color = get_file_color(result->info.st_mode);
                printf("%s%s%s\n", color, result->path, COLOR_RESET);
            } else {
                printf("%s\n", result->path);
            }
        }
    }
    
    if (result_count > 0 && !opts->show_stats) {
        printf("\n");
    }
}

// tkfind主函数
int tkfind_main(int argc, char **argv) {
    Options opts;
    init_options(&opts);
    
    int parse_result = parse_options(argc, argv, &opts);
    if (parse_result <= 0) {
        return parse_result == -1 ? 1 : 0;
    }
    
    if (opts.help) {
        show_help();
        return 0;
    }
    
    if (opts.version) {
        show_version();
        return 0;
    }
    
    // 显示搜索条件
    if (opts.color_output) {
        color_println(COLOR_BRIGHT_CYAN, "🔍 搜索条件:");
        printf("══════════════════════════════════════════════════════════════\n");
        
        if (opts.name_pattern) {
            color_print(COLOR_BRIGHT_GREEN, "文件名: ");
            printf("%s", opts.name_pattern);
            if (opts.regex_mode) printf(" (正则)");
            if (opts.ignore_case) printf(" (忽略大小写)");
            printf("\n");
        }
        
        if (opts.type_filter) {
            color_print(COLOR_BRIGHT_GREEN, "文件类型: ");
            printf("%s\n", opts.type_filter);
        }
        
        if (opts.size_filter) {
            color_print(COLOR_BRIGHT_GREEN, "文件大小: ");
            printf("%s\n", opts.size_filter);
        }
        
        if (opts.time_filter) {
            color_print(COLOR_BRIGHT_GREEN, "修改时间: ");
            printf("%s天\n", opts.time_filter);
        }
        
        if (opts.content_pattern) {
            color_print(COLOR_BRIGHT_GREEN, "内容: ");
            printf("%s", opts.content_pattern);
            if (opts.ignore_case) printf(" (忽略大小写)");
            printf("\n");
        }
        
        printf("\n");
    }
    
    // 搜索所有路径
    SearchResult *results = NULL;
    int result_count = 0;
    int total_files = 0;
    int total_dirs = 0;
    
    for (int i = 0; i < opts.path_count; i++) {
        const char *path = opts.paths[i];
        
        if (!file_exists(path)) {
            print_warning("路径不存在: %s", path);
            continue;
        }
        
        struct stat st;
        if (stat(path, &st) == -1) {
            print_warning("无法访问: %s", path);
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // 搜索目录
            int dir_files = 0, dir_dirs = 0;
            search_directory(path, &opts, 0, -1, &results, &result_count, 
                           &dir_files, &dir_dirs);
            total_files += dir_files;
            total_dirs += dir_dirs;
        } else {
            // 单个文件
            if (match_name(path, opts.name_pattern, opts.regex_mode, opts.ignore_case) &&
                match_type(opts.type_filter, st.st_mode) &&
                match_size(opts.size_filter, st.st_size) &&
                match_time(opts.time_filter, st.st_mtime)) {
                
                if (opts.content_pattern) {
                    int match_count = 0;
                    if (search_content(path, opts.content_pattern, opts.ignore_case, &match_count) && 
                        match_count > 0) {
                        result_count++;
                        results = realloc(results, sizeof(SearchResult) * result_count);
                        SearchResult *result = &results[result_count - 1];
                        result->path = strdup(path);
                        result->info = st;
                        result->matches = match_count;
                        total_files++;
                    }
                } else {
                    result_count++;
                    results = realloc(results, sizeof(SearchResult) * result_count);
                    SearchResult *result = &results[result_count - 1];
                    result->path = strdup(path);
                    result->info = st;
                    result->matches = 0;
                    total_files++;
                }
            }
        }
    }
    
    // 显示结果
    show_results(results, result_count, &opts, total_files, total_dirs);
    
    // 清理
    for (int i = 0; i < result_count; i++) {
        free(results[i].path);
    }
    free(results);
    if (opts.paths) free(opts.paths);
    
    return 0;
}