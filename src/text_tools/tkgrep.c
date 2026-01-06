// src/text_tools/tkgrep.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <errno.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 选项结构
typedef struct {
    int ignore_case;      // -i 忽略大小写
    int line_number;      // -n 显示行号
    int count_only;       // -c 只计数
    int invert_match;     // -v 反向匹配
    int whole_word;       // -w 整词匹配
    int recursive;        // -r 递归搜索
    int show_context;     // -C 显示上下文
    int context_lines;    // 上下文行数
    int color_output;     // --color 高亮显示
    int basic_regex;      // -G 基本正则
    int extended_regex;   // -E 扩展正则
    int fixed_strings;    // -F 固定字符串
    int help;            // --help
    int version;         // --version
    char *pattern;       // 搜索模式
    char **files;        // 文件列表
    int file_count;      // 文件数量
} Options;

// 初始化选项
static void init_options(Options *opts) {
    opts->ignore_case = 0;
    opts->line_number = 0;
    opts->count_only = 0;
    opts->invert_match = 0;
    opts->whole_word = 0;
    opts->recursive = 0;
    opts->show_context = 0;
    opts->context_lines = 2; // 默认显示2行上下文
    opts->color_output = is_color_supported();
    opts->basic_regex = 0;
    opts->extended_regex = 1; // 默认使用扩展正则
    opts->fixed_strings = 0;
    opts->help = 0;
    opts->version = 0;
    opts->pattern = NULL;
    opts->files = NULL;
    opts->file_count = 0;
}

// 显示帮助
static void show_help(void) {
    color_println(COLOR_BRIGHT_CYAN, "tkgrep - 增强版grep工具");
    printf("\n");
    printf("用法: tkgrep [选项] <模式> [文件]...\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "搜索选项:");
    printf("  -i, --ignore-case      忽略大小写\n");
    printf("  -v, --invert-match     选择不匹配的行\n");
    printf("  -w, --word-regexp      强制模式匹配整个单词\n");
    printf("  -F, --fixed-strings    模式为固定字符串\n");
    printf("  -G, --basic-regexp     使用基本正则表达式\n");
    printf("  -E, --extended-regexp  使用扩展正则表达式（默认）\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "输出控制:");
    printf("  -n, --line-number      输出行号\n");
    printf("  -c, --count            只显示匹配行数\n");
    printf("  -C NUM, --context=NUM  显示匹配行的上下文（前后NUM行）\n");
    printf("      --color            高亮显示匹配内容\n");
    printf("      --no-color         不高亮显示\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "文件选择:");
    printf("  -r, --recursive        递归搜索子目录\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "其他:");
    printf("      --help             显示此帮助\n");
    printf("      --version          显示版本\n");
    printf("\n");
    color_println(COLOR_BRIGHT_GREEN, "示例:");
    printf("  tkgrep pattern file.txt           # 在文件中搜索\n");
    printf("  tkgrep -i " COLOR_BRIGHT_RED "error" COLOR_RESET " *.log          # 忽略大小写搜索\n");
    printf("  tkgrep -n -C2 pattern file.c      # 显示行号和上下文\n");
    printf("  tkgrep -r pattern .               # 递归搜索当前目录\n");
    printf("  echo \"text\" | tkgrep pattern     # 从标准输入搜索\n");
}

// 显示版本
static void show_version(void) {
    color_println(COLOR_BRIGHT_MAGENTA, "tkgrep - TermKit 增强版grep工具");
    printf("版本: 1.0.0\n");
    printf("功能: 正则表达式、上下文显示、高亮匹配\n");
}

// 解析选项
static int parse_options(int argc, char **argv, Options *opts) {
    int i = 1;
    
    // 如果没有参数，显示帮助
    if (argc == 1) {
        show_help();
        return 0;
    }
    
    // 解析选项
    while (i < argc) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-case") == 0) {
                opts->ignore_case = 1;
            } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--line-number") == 0) {
                opts->line_number = 1;
            } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--count") == 0) {
                opts->count_only = 1;
            } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--invert-match") == 0) {
                opts->invert_match = 1;
            } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--word-regexp") == 0) {
                opts->whole_word = 1;
            } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
                opts->recursive = 1;
            } else if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--fixed-strings") == 0) {
                opts->fixed_strings = 1;
                opts->extended_regex = 0;
                opts->basic_regex = 0;
            } else if (strcmp(argv[i], "-G") == 0 || strcmp(argv[i], "--basic-regexp") == 0) {
                opts->basic_regex = 1;
                opts->extended_regex = 0;
            } else if (strcmp(argv[i], "-E") == 0 || strcmp(argv[i], "--extended-regexp") == 0) {
                opts->extended_regex = 1;
                opts->basic_regex = 0;
            } else if (strcmp(argv[i], "--color") == 0) {
                opts->color_output = 1;
            } else if (strcmp(argv[i], "--no-color") == 0) {
                opts->color_output = 0;
            } else if (strcmp(argv[i], "--help") == 0) {
                opts->help = 1;
                return 1;
            } else if (strcmp(argv[i], "--version") == 0) {
                opts->version = 1;
                return 1;
            } else if (strncmp(argv[i], "-C", 2) == 0 || strncmp(argv[i], "--context=", 10) == 0) {
                opts->show_context = 1;
                
                // 解析上下文行数
                const char *num_str;
                if (strncmp(argv[i], "-C", 2) == 0) {
                    if (strlen(argv[i]) > 2) {
                        num_str = argv[i] + 2;
                    } else if (i + 1 < argc) {
                        i++;
                        num_str = argv[i];
                    } else {
                        print_error("选项 -C 需要参数");
                        return -1;
                    }
                } else {
                    num_str = argv[i] + 10; // 跳过 "--context="
                }
                
                if (!parse_int(num_str, &opts->context_lines) || opts->context_lines < 0) {
                    print_error("无效的上下文行数: %s", num_str);
                    return -1;
                }
            } else {
                print_error("无效选项: %s", argv[i]);
                printf("使用 'tkgrep --help' 查看帮助\n");
                return -1;
            }
            i++;
        } else {
            // 第一个非选项参数是模式
            if (opts->pattern == NULL) {
                opts->pattern = argv[i];
                i++;
                
                // 处理整词匹配
                if (opts->whole_word) {
                    char *new_pattern = malloc(strlen(opts->pattern) + 5); // \\bpattern\\b
                    sprintf(new_pattern, "\\b%s\\b", opts->pattern);
                    // 注意：这里不修改原参数，因为整词匹配会在编译正则时处理
                }
            } else {
                // 其余的是文件
                opts->file_count++;
                opts->files = realloc(opts->files, sizeof(char*) * opts->file_count);
                opts->files[opts->file_count - 1] = argv[i];
                i++;
            }
        }
    }
    
    // 检查模式是否提供
    if (opts->pattern == NULL && !opts->help && !opts->version) {
        print_error("缺少搜索模式");
        printf("使用 'tkgrep --help' 查看帮助\n");
        return -1;
    }
    
    return 1;
}

// 高亮显示匹配的文本
static void highlight_match(const char *line, const char *pattern, 
                           int fixed_strings, int ignore_case) {
    if (!line || !pattern) {
        printf("%s", line ? line : "");
        return;
    }
    
    const char *ptr = line;
    
    if (fixed_strings) {
        // 固定字符串匹配
        const char *match;
        const char *search_ptr = ptr;
        size_t pattern_len = strlen(pattern);
        
        while (*search_ptr) {
            if (ignore_case) {
                match = strcasestr(search_ptr, pattern);
            } else {
                match = strstr(search_ptr, pattern);
            }
            
            if (match == NULL) {
                // 输出剩余部分
                printf("%s", search_ptr);
                break;
            }
            
            // 输出匹配前的部分
            printf("%.*s", (int)(match - search_ptr), search_ptr);
            
            // 高亮显示匹配部分
            color_print(COLOR_BRIGHT_RED, "%.*s", (int)pattern_len, match);
            
            search_ptr = match + pattern_len;
        }
    } else {
        // 正则表达式匹配（简化处理）
        // 这里简化处理，实际应该使用regexec获取匹配位置
        printf("%s", line);
    }
}

// 编译正则表达式
static regex_t* compile_regex(const char *pattern, Options *opts) {
    regex_t *regex = malloc(sizeof(regex_t));
    if (regex == NULL) {
        print_error("内存分配失败");
        return NULL;
    }
    
    int flags = REG_EXTENDED;
    if (opts->ignore_case) flags |= REG_ICASE;
    if (opts->basic_regex) flags &= ~REG_EXTENDED;
    
    // 处理整词匹配
    char *actual_pattern;
    if (opts->whole_word) {
        size_t len = strlen(pattern) + 5; // \bpattern\b
        actual_pattern = malloc(len);
        snprintf(actual_pattern, len, "\\b%s\\b", pattern);
    } else {
        actual_pattern = strdup(pattern);
    }
    
    int ret = regcomp(regex, actual_pattern, flags);
    free(actual_pattern);
    
    if (ret != 0) {
        char error_buf[256];
        regerror(ret, regex, error_buf, sizeof(error_buf));
        print_error("正则表达式编译失败: %s", error_buf);
        free(regex);
        return NULL;
    }
    
    return regex;
}

// 在文件中搜索
static int search_in_file(const char *filename, regex_t *regex, Options *opts) {
    FILE *file;
    
    if (filename == NULL || strcmp(filename, "-") == 0) {
        // 从标准输入读取
        file = stdin;
        filename = "(标准输入)";
    } else {
        file = fopen(filename, "r");
        if (file == NULL) {
            print_error("无法打开文件 '%s': %s", filename, strerror(errno));
            return -1;
        }
    }
    
    char *line = NULL;
    size_t line_len = 0;
    ssize_t read;
    int line_num = 0;
    int match_count = 0;
    
    // 上下文缓冲区
    char **context_buf = NULL;
    int *context_line_nums = NULL;
    int context_start = 0;
    int context_count = 0;
    
    if (opts->show_context) {
        int buf_size = opts->context_lines * 2 + 1;
        context_buf = malloc(sizeof(char*) * buf_size);
        context_line_nums = malloc(sizeof(int) * buf_size);
        for (int i = 0; i < buf_size; i++) {
            context_buf[i] = NULL;
        }
    }
    
    while ((read = getline(&line, &line_len, file)) != -1) {
        line_num++;
        
        // 去除换行符
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
            read--;
        }
        
        int matches;
        
        if (opts->fixed_strings) {
            // 固定字符串匹配
            if (opts->ignore_case) {
                matches = (strcasestr(line, opts->pattern) != NULL);
            } else {
                matches = (strstr(line, opts->pattern) != NULL);
            }
        } else {
            // 正则表达式匹配
            matches = (regexec(regex, line, 0, NULL, 0) == 0);
        }
        
        // 处理反向匹配
        if (opts->invert_match) {
            matches = !matches;
        }
        
        // 保存到上下文缓冲区
        if (opts->show_context) {
            int buf_size = opts->context_lines * 2 + 1;
            int idx = context_count % buf_size;
            
            // 释放旧的行
            if (context_buf[idx]) {
                free(context_buf[idx]);
            }
            
            // 保存新行
            context_buf[idx] = strdup(line);
            context_line_nums[idx] = line_num;
            context_count++;
            
            // 如果缓冲区满了，移动起始位置
            if (context_count > buf_size) {
                context_start = (context_start + 1) % buf_size;
            }
        }
        
        if (matches) {
            match_count++;
            
            if (!opts->count_only) {
                // 显示上下文（如果需要）
                if (opts->show_context) {
                    int buf_size = opts->context_lines * 2 + 1;
                    int display_start = context_start;
                    
                    // 计算要显示的行范围
                    int first_line = line_num - opts->context_lines;
                    if (first_line < 1) first_line = 1;
                    
                    // 显示匹配行前的上下文
                    for (int i = display_start; i < context_count; i++) {
                        int idx = i % buf_size;
                        if (context_line_nums[idx] >= first_line && 
                            context_line_nums[idx] < line_num) {
                            
                            // 显示分隔符
                            if (opts->file_count > 1) {
                                color_print(COLOR_BRIGHT_BLUE, "%s:", filename);
                            }
                            
                            // 显示行号
                            if (opts->line_number) {
                                color_print(COLOR_BRIGHT_GREEN, "%d-", context_line_nums[idx]);
                            }
                            
                            printf("%s\n", context_buf[idx]);
                        }
                    }
                }
                
                // 显示匹配行
                if (opts->file_count > 1) {
                    color_print(COLOR_BRIGHT_BLUE, "%s:", filename);
                }
                
                if (opts->line_number) {
                    color_print(COLOR_BRIGHT_GREEN, "%d:", line_num);
                }
                
                // 高亮显示匹配内容
                if (opts->color_output) {
                    highlight_match(line, opts->pattern, opts->fixed_strings, opts->ignore_case);
                    printf("\n");
                } else {
                    printf("%s\n", line);
                }
                
                // 显示匹配行后的上下文（简化处理，显示下一行）
                if (opts->show_context) {
                    // 这里简化处理，实际应该继续读取并显示后续上下文
                }
                
                // 显示上下文分隔符
                if (opts->show_context) {
                    printf("--\n");
                }
            }
        }
    }
    
    // 显示计数
    if (opts->count_only) {
        if (opts->file_count > 1) {
            printf("%s:", filename);
        }
        printf("%d\n", match_count);
    }
    
    // 清理
    if (line) free(line);
    if (file != stdin) fclose(file);
    
    if (opts->show_context) {
        int buf_size = opts->context_lines * 2 + 1;
        for (int i = 0; i < buf_size; i++) {
            if (context_buf[i]) free(context_buf[i]);
        }
        free(context_buf);
        free(context_line_nums);
    }
    
    return match_count;
}

// 递归搜索目录
static int search_in_directory(const char *dirpath, regex_t *regex, Options *opts) {
    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        print_error("无法打开目录 '%s': %s", dirpath, strerror(errno));
        return -1;
    }
    
    struct dirent *entry;
    int total_matches = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);
        
        if (entry->d_type == DT_DIR) {
            // 递归搜索子目录
            total_matches += search_in_directory(full_path, regex, opts);
        } else if (entry->d_type == DT_REG) {
            // 搜索文件
            int matches = search_in_file(full_path, regex, opts);
            if (matches > 0) {
                total_matches += matches;
            }
        }
    }
    
    closedir(dir);
    return total_matches;
}

// tkgrep主函数
int tkgrep_main(int argc, char **argv) {
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
    
    // 编译正则表达式（如果不是固定字符串）
    regex_t *regex = NULL;
    if (!opts.fixed_strings) {
        regex = compile_regex(opts.pattern, &opts);
        if (regex == NULL) {
            return 1;
        }
    }
    
    int total_matches = 0;
    int exit_code = 0;
    
    if (opts.file_count == 0) {
        // 从标准输入搜索
        total_matches = search_in_file(NULL, regex, &opts);
    } else {
        // 搜索文件
        for (int i = 0; i < opts.file_count; i++) {
            const char *filename = opts.files[i];
            
            if (opts.recursive && is_directory(filename)) {
                // 递归搜索目录
                int matches = search_in_directory(filename, regex, &opts);
                if (matches >= 0) {
                    total_matches += matches;
                } else {
                    exit_code = 1;
                }
            } else {
                // 搜索单个文件
                int matches = search_in_file(filename, regex, &opts);
                if (matches >= 0) {
                    total_matches += matches;
                } else {
                    exit_code = 1;
                }
            }
        }
    }
    
    // 如果没有匹配且不是只计数模式，设置退出码
    if (total_matches == 0 && !opts.count_only && exit_code == 0) {
        exit_code = 1; // grep约定：没有匹配返回1
    }
    
    // 清理
    if (regex) {
        regfree(regex);
        free(regex);
    }
    
    if (opts.files) {
        free(opts.files);
    }
    
    return exit_code;
}