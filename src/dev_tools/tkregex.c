// src/dev_tools/tkregex.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "../common/colors.h"
#include "../common/utils.h"
#include "../common/progress.h"

#define MAX_PATTERN 512
#define MAX_LINE 4096
#define MAX_MATCHES 100

typedef struct {
    char pattern[MAX_PATTERN];
    int case_sensitive;
    int whole_word;
    int line_numbers;
    int count_only;
    int invert_match;
    int recursive;
    int show_stats;
    int color_output;
    char filename[256];
} RegexConfig;

void print_help() {
    printf("tkregex - 增强正则表达式工具\n\n");
    printf("用法:\n");
    printf("  tkregex [选项] <模式> [文件...]\n\n");
    printf("选项:\n");
    printf("  -i           忽略大小写\n");
    printf("  -w           整词匹配\n");
    printf("  -n           显示行号\n");
    printf("  -c           只显示匹配计数\n");
    printf("  -v           反向匹配（显示不匹配的行）\n");
    printf("  -r           递归搜索目录\n");
    printf("  -s           显示统计信息\n");
    printf("  -C           彩色输出\n");
    printf("  -t           测试模式\n");
    printf("  -h           显示帮助\n\n");
    
    printf("示例:\n");
    printf("  tkregex 'error' log.txt\n");
    printf("  tkregex -ri 'warning|error' logs/\n");
    printf("  tkregex -n '^\\d+' data.txt\n");
    printf("  tkregex -t '^[a-z]+$'\n");
}

void print_regex_info() {
    printf("\n常用正则表达式:\n");
    color_println(COLOR_CYAN, "基础:");
    printf("  .          任意字符\n");
    printf("  ^          行首\n");
    printf("  $          行尾\n");
    printf("  *          0次或多次\n");
    printf("  +          1次或多次\n");
    printf("  ?          0次或1次\n");
    printf("  [abc]      a、b或c\n");
    printf("  [^abc]     非a、b、c\n");
    printf("  [a-z]      a到z\n");
    printf("  \\d         数字\n");
    printf("  \\w         单词字符\n");
    printf("  \\s         空白字符\n\n");
    
    color_println(COLOR_CYAN, "示例:");
    printf("  邮箱: ^[\\w-\\.]+@([\\w-]+\\.)+[\\w-]{2,4}$\n");
    printf("  IP地址: ^(\\d{1,3}\\.){3}\\d{1,3}$\n");
    printf("  日期: ^\\d{4}-\\d{2}-\\d{2}$\n");
    printf("  时间: ^\\d{2}:\\d{2}:\\d{2}$\n");
}

// 编译正则表达式
int compile_regex(regex_t *regex, const char *pattern, int case_sensitive, int whole_word) {
    char full_pattern[MAX_PATTERN];
    int flags = REG_EXTENDED | REG_NEWLINE;
    
    if (!case_sensitive) {
        flags |= REG_ICASE;
    }
    
    if (whole_word) {
        snprintf(full_pattern, sizeof(full_pattern), "\\b%s\\b", pattern);
    } else {
        snprintf(full_pattern, sizeof(full_pattern), "%s", pattern);
    }
    
    int ret = regcomp(regex, full_pattern, flags);
    if (ret != 0) {
        char error_buf[100];
        regerror(ret, regex, error_buf, sizeof(error_buf));
        print_error("正则表达式错误: %s", error_buf);
        return 0;
    }
    
    return 1;
}

// 高亮显示匹配的文本
void highlight_match(const char *line, regmatch_t *matches, int num_matches, int line_num, int show_line_num) {
    if (show_line_num) {
        color_print(COLOR_BRIGHT_BLACK, "%6d:", line_num);
    }
    
    int last_pos = 0;
    for (int i = 0; i < num_matches; i++) {
        if (matches[i].rm_so == -1) break;
        
        // 打印非匹配部分
        if (last_pos < matches[i].rm_so) {
            printf("%.*s", (int)(matches[i].rm_so - last_pos), line + last_pos);
        }
        
        // 高亮匹配部分
        color_print(COLOR_BRIGHT_RED, "%.*s", 
                   (int)(matches[i].rm_eo - matches[i].rm_so), 
                   line + matches[i].rm_so);
        
        last_pos = matches[i].rm_eo;
    }
    
    // 打印剩余部分
    if (last_pos < (int)strlen(line)) {
        printf("%s", line + last_pos);
    }
    
    if (line[strlen(line)-1] != '\n') {
        printf("\n");
    }
}

// 在单个文件中搜索
int search_in_file(const char *filename, regex_t *regex, RegexConfig *config, int *total_matches) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        if (!config->count_only) {
            print_error("无法打开文件: %s", filename);
        }
        return 0;
    }
    
    char line[MAX_LINE];
    int line_num = 0;
    int file_matches = 0;
    regmatch_t matches[MAX_MATCHES];
    
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        line[strcspn(line, "\n")] = '\0';  // 移除换行符
        
        int ret = regexec(regex, line, MAX_MATCHES, matches, 0);
        int matched = (ret == 0);
        
        if (config->invert_match) {
            matched = !matched;
        }
        
        if (matched) {
            file_matches++;
            *total_matches = *total_matches + 1;
            
            if (!config->count_only) {
                if (config->color_output && !config->invert_match) {
                    highlight_match(line, matches, MAX_MATCHES, line_num, config->line_numbers);
                } else {
                    if (config->line_numbers) {
                        color_print(COLOR_BRIGHT_BLACK, "%6d:", line_num);
                    }
                    printf("%s\n", line);
                }
            }
        }
    }
    
    fclose(file);
    
    if (config->count_only && file_matches > 0) {
        if (strcmp(filename, "-") != 0) {
            printf("%s:%d\n", filename, file_matches);
        } else {
            printf("%d\n", file_matches);
        }
    }
    
    return file_matches;
}

// 递归搜索目录
int search_directory(const char *dirname, regex_t *regex, RegexConfig *config, int *total_matches) {
    DIR *dir = opendir(dirname);
    if (!dir) {
        print_error("无法打开目录: %s", dirname);
        return 0;
    }
    
    struct dirent *entry;
    char path[1024];
    int dir_matches = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);
        
        struct stat statbuf;
        if (stat(path, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                if (config->recursive) {
                    dir_matches += search_directory(path, regex, config, total_matches);
                }
            } else if (S_ISREG(statbuf.st_mode)) {
                if (!config->count_only) {
                    color_println(COLOR_CYAN, "\n文件: %s", path);
                }
                dir_matches += search_in_file(path, regex, config, total_matches);
            }
        }
    }
    
    closedir(dir);
    return dir_matches;
}

// 测试正则表达式
void test_pattern(const char *pattern) {
    regex_t regex;
    
    if (!compile_regex(&regex, pattern, 1, 0)) {
        return;
    }
    
    printf("\n正则表达式测试模式\n");
    printf("模式: %s\n", pattern);
    printf("\n输入文本进行测试 (输入空行退出):\n");
    
    char input[MAX_LINE];
    int test_num = 1;
    
    while (1) {
        printf("\n测试 %d> ", test_num++);
        if (!fgets(input, sizeof(input), stdin)) break;
        
        trim_string(input);
        if (strlen(input) == 0) break;
        
        regmatch_t matches[MAX_MATCHES];
        int ret = regexec(&regex, input, MAX_MATCHES, matches, 0);
        
        if (ret == 0) {
            color_print(COLOR_GREEN, "匹配成功 ");
            printf("位置: ");
            for (int i = 0; i < MAX_MATCHES; i++) {
                if (matches[i].rm_so == -1) break;
                printf("[%d-%d]", matches[i].rm_so, matches[i].rm_eo - 1);
            }
            printf("\n");
            
            // 高亮显示
            highlight_match(input, matches, MAX_MATCHES, 0, 0);
        } else if (ret == REG_NOMATCH) {
            color_println(COLOR_RED, "匹配失败");
        } else {
            char error_buf[100];
            regerror(ret, &regex, error_buf, sizeof(error_buf));
            print_error("匹配错误: %s", error_buf);
        }
    }
    
    regfree(&regex);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        print_regex_info();
        return 1;
    }
    
    RegexConfig config = {
        .case_sensitive = 1,
        .whole_word = 0,
        .line_numbers = 0,
        .count_only = 0,
        .invert_match = 0,
        .recursive = 0,
        .show_stats = 0,
        .color_output = is_color_supported(),
        .filename = ""
    };
    
    int test_mode = 0;
    char pattern[MAX_PATTERN] = {0};
    
    // 解析选项
    int opt;
    while ((opt = getopt(argc, argv, "iwncvrCsht")) != -1) {
        switch (opt) {
            case 'i':
                config.case_sensitive = 0;
                break;
            case 'w':
                config.whole_word = 1;
                break;
            case 'n':
                config.line_numbers = 1;
                break;
            case 'c':
                config.count_only = 1;
                break;
            case 'v':
                config.invert_match = 1;
                break;
            case 'r':
                config.recursive = 1;
                break;
            case 's':
                config.show_stats = 1;
                break;
            case 'C':
                config.color_output = 1;
                break;
            case 't':
                test_mode = 1;
                break;
            case 'h':
                print_help();
                print_regex_info();
                return 0;
            default:
                print_help();
                return 1;
        }
    }
    
    // 获取模式
    if (optind < argc) {
        strncpy(pattern, argv[optind], sizeof(pattern) - 1);
        optind++;
    } else {
        print_error("需要指定正则表达式模式");
        return 1;
    }
    
    // 测试模式
    if (test_mode) {
        test_pattern(pattern);
        return 0;
    }
    
    // 编译正则表达式
    regex_t regex;
    if (!compile_regex(&regex, pattern, config.case_sensitive, config.whole_word)) {
        return 1;
    }
    
    // 获取文件列表
    char **files = NULL;
    int num_files = 0;
    
    if (optind < argc) {
        // 有指定文件
        num_files = argc - optind;
        files = &argv[optind];
    } else {
        // 没有指定文件，从标准输入读取
        num_files = 1;
        static char *stdin_files[] = {"-"};
        files = stdin_files;
    }
    
    // 开始搜索
    int total_matches = 0;
    clock_t start_time = clock();
    
    for (int i = 0; i < num_files; i++) {
        struct stat statbuf;
        int is_file = 0;
        int is_dir = 0;
        
        if (strcmp(files[i], "-") == 0) {
            is_file = 1;
        } else if (stat(files[i], &statbuf) == 0) {
            is_file = S_ISREG(statbuf.st_mode);
            is_dir = S_ISDIR(statbuf.st_mode);
        } else {
            print_error("文件不存在: %s", files[i]);
            continue;
        }
        
        if (is_file) {
            if (!config.count_only && num_files > 1) {
                color_println(COLOR_CYAN, "\n文件: %s", files[i]);
            }
            search_in_file(files[i], &regex, &config, &total_matches);
        } else if (is_dir) {
            search_directory(files[i], &regex, &config, &total_matches);
        }
    }
    
    clock_t end_time = clock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    // 显示统计信息
    if (config.show_stats || config.count_only) {
        printf("\n");
        color_println(COLOR_CYAN, "搜索统计:");
        printf("模式: %s\n", pattern);
        printf("文件数: %d\n", num_files);
        printf("总匹配数: %d\n", total_matches);
        printf("耗时: %.3f 秒\n", elapsed);
    }
    
    regfree(&regex);
    return (total_matches > 0) ? 0 : 1;
}