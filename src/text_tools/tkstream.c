// src/text_tools/tkstream.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <regex.h>
#include <time.h>
#include <sys/stat.h>
#include "../common/colors.h"
#include "../common/utils.h"
#include "../common/progress.h"

#define BUFFER_SIZE 8192
#define MAX_LINE 4096
#define MAX_FILTERS 20
#define MAX_COLUMNS 100

typedef enum {
    FILTER_NONE,
    FILTER_GREP,
    FILTER_SED,
    FILTER_AWK,
    FILTER_CUT,
    FILTER_SORT,
    FILTER_UNIQ,
    FILTER_WC,
    FILTER_HEAD,
    FILTER_TAIL,
    FILTER_TR,
    FILTER_TRIM,
    FILTER_UPPER,
    FILTER_LOWER,
    FILTER_REVERSE,
    FILTER_JOIN,
    FILTER_SPLIT,
    FILTER_COUNT
} FilterType;

typedef struct {
    FilterType type;
    char pattern[256];
    char delimiter[16];
    int field;
    int start;
    int end;
    int numeric;
    int reverse;
    int ignore_case;
    int whole_word;
} Filter;

typedef struct {
    Filter filters[MAX_FILTERS];
    int filter_count;
    char input_file[256];
    char output_file[256];
    int verbose;
    int show_stats;
    int interactive;
    int batch_size;
    char delimiter[16];
    int skip_header;
    int column_output;
} Config;

void print_help() {
    printf("tkstream - 流式文本处理工具\n\n");
    printf("用法:\n");
    printf("  tkstream [选项] [过滤器...] [文件]\n\n");
    
    printf("选项:\n");
    printf("  -i <文件>    输入文件（默认：标准输入）\n");
    printf("  -o <文件>    输出文件（默认：标准输出）\n");
    printf("  -d <分隔符>  字段分隔符（默认：空格）\n");
    printf("  -H           跳过第一行（标题行）\n");
    printf("  -C           列式输出\n");
    printf("  -v           详细输出\n");
    printf("  -s           显示统计信息\n");
    printf("  -b <行数>    批处理大小（默认：1000）\n");
    printf("  -h           显示帮助\n\n");
    
    printf("过滤器（可以组合使用）:\n");
    printf("  grep <模式>          包含模式的行\n");
    printf("  grep -v <模式>       不包含模式的行\n");
    printf("  sed 's/old/new/'     替换文本\n");
    printf("  cut -f N             提取第N列\n");
    printf("  sort [-n] [-r]       排序（-n数字排序，-r反向）\n");
    printf("  uniq [-c]            去重（-c计数）\n");
    printf("  wc                   统计行数/词数/字符数\n");
    printf("  head -n N            前N行\n");
    printf("  tail -n N            后N行\n");
    printf("  tr 'a-z' 'A-Z'       字符转换\n");
    printf("  trim                 去除首尾空白\n");
    printf("  upper                转换为大写\n");
    printf("  lower                转换为小写\n");
    printf("  reverse              反转行\n");
    printf("  join -d ','          用分隔符连接字段\n");
    printf("  split -d ','         用分隔符分割字段\n");
    printf("  count                统计出现次数\n\n");
    
    printf("示例:\n");
    printf("  cat log.txt | tkstream grep 'error' | head -n 10\n");
    printf("  tkstream -i data.csv -d ',' cut -f 1,3 sort -n\n");
    printf("  tkstream -i text.txt trim upper | grep 'THE'\n");
    printf("  tkstream -i log.txt grep -v 'DEBUG' | wc\n");
}

// 解析过滤器参数
int parse_filter(int argc, char *argv[], int *index, Filter *filter) {
    if (*index >= argc) return 0;
    
    char *cmd = argv[*index];
    filter->type = FILTER_NONE;
    filter->pattern[0] = '\0';
    strcpy(filter->delimiter, " ");
    filter->field = 0;
    filter->start = 0;
    filter->end = 0;
    filter->numeric = 0;
    filter->reverse = 0;
    filter->ignore_case = 0;
    filter->whole_word = 0;
    
    if (strcmp(cmd, "grep") == 0) {
        filter->type = FILTER_GREP;
        (*index)++;
        
        // 检查选项
        if (*index < argc && strcmp(argv[*index], "-v") == 0) {
            filter->reverse = 1;
            (*index)++;
        }
        if (*index < argc && strcmp(argv[*index], "-i") == 0) {
            filter->ignore_case = 1;
            (*index)++;
        }
        if (*index < argc && strcmp(argv[*index], "-w") == 0) {
            filter->whole_word = 1;
            (*index)++;
        }
        
        if (*index < argc) {
            strncpy(filter->pattern, argv[*index], sizeof(filter->pattern) - 1);
            (*index)++;
            return 1;
        }
    }
    else if (strcmp(cmd, "sed") == 0) {
        filter->type = FILTER_SED;
        (*index)++;
        if (*index < argc) {
            strncpy(filter->pattern, argv[*index], sizeof(filter->pattern) - 1);
            (*index)++;
            return 1;
        }
    }
    else if (strcmp(cmd, "cut") == 0) {
        filter->type = FILTER_CUT;
        (*index)++;
        if (*index < argc && strcmp(argv[*index], "-f") == 0) {
            (*index)++;
            if (*index < argc) {
                // 支持多个字段，如 1,3,5
                strncpy(filter->pattern, argv[*index], sizeof(filter->pattern) - 1);
                (*index)++;
                return 1;
            }
        }
    }
    else if (strcmp(cmd, "sort") == 0) {
        filter->type = FILTER_SORT;
        (*index)++;
        while (*index < argc && argv[*index][0] == '-') {
            if (strcmp(argv[*index], "-n") == 0) {
                filter->numeric = 1;
            } else if (strcmp(argv[*index], "-r") == 0) {
                filter->reverse = 1;
            } else if (strcmp(argv[*index], "-i") == 0) {
                filter->ignore_case = 1;
            }
            (*index)++;
        }
        return 1;
    }
    else if (strcmp(cmd, "uniq") == 0) {
        filter->type = FILTER_UNIQ;
        (*index)++;
        if (*index < argc && strcmp(argv[*index], "-c") == 0) {
            filter->numeric = 1;  // 使用numeric标记计数模式
            (*index)++;
        }
        return 1;
    }
    else if (strcmp(cmd, "wc") == 0) {
        filter->type = FILTER_WC;
        (*index)++;
        return 1;
    }
    else if (strcmp(cmd, "head") == 0) {
        filter->type = FILTER_HEAD;
        (*index)++;
        if (*index < argc && strcmp(argv[*index], "-n") == 0) {
            (*index)++;
            if (*index < argc) {
                filter->end = atoi(argv[*index]);  // 使用end作为行数
                (*index)++;
                return 1;
            }
        }
    }
    else if (strcmp(cmd, "tail") == 0) {
        filter->type = FILTER_TAIL;
        (*index)++;
        if (*index < argc && strcmp(argv[*index], "-n") == 0) {
            (*index)++;
            if (*index < argc) {
                filter->end = atoi(argv[*index]);
                (*index)++;
                return 1;
            }
        }
    }
    else if (strcmp(cmd, "tr") == 0) {
        filter->type = FILTER_TR;
        (*index)++;
        if (*index < argc) {
            strncpy(filter->pattern, argv[*index], sizeof(filter->pattern) - 1);
            (*index)++;
            if (*index < argc) {
                strncpy(filter->delimiter, argv[*index], sizeof(filter->delimiter) - 1);
                (*index)++;
                return 1;
            }
        }
    }
    else if (strcmp(cmd, "trim") == 0) {
        filter->type = FILTER_TRIM;
        (*index)++;
        return 1;
    }
    else if (strcmp(cmd, "upper") == 0) {
        filter->type = FILTER_UPPER;
        (*index)++;
        return 1;
    }
    else if (strcmp(cmd, "lower") == 0) {
        filter->type = FILTER_LOWER;
        (*index)++;
        return 1;
    }
    else if (strcmp(cmd, "reverse") == 0) {
        filter->type = FILTER_REVERSE;
        (*index)++;
        return 1;
    }
    else if (strcmp(cmd, "join") == 0) {
        filter->type = FILTER_JOIN;
        (*index)++;
        if (*index < argc && strcmp(argv[*index], "-d") == 0) {
            (*index)++;
            if (*index < argc) {
                strncpy(filter->delimiter, argv[*index], sizeof(filter->delimiter) - 1);
                (*index)++;
                return 1;
            }
        }
    }
    else if (strcmp(cmd, "split") == 0) {
        filter->type = FILTER_SPLIT;
        (*index)++;
        if (*index < argc && strcmp(argv[*index], "-d") == 0) {
            (*index)++;
            if (*index < argc) {
                strncpy(filter->delimiter, argv[*index], sizeof(filter->delimiter) - 1);
                (*index)++;
                return 1;
            }
        }
    }
    else if (strcmp(cmd, "count") == 0) {
        filter->type = FILTER_COUNT;
        (*index)++;
        return 1;
    }
    
    return 0;
}

// 应用trim过滤器
void apply_trim(char *line) {
    trim_string(line);
}

// 应用upper过滤器
void apply_upper(char *line) {
    for (int i = 0; line[i]; i++) {
        line[i] = toupper(line[i]);
    }
}

// 应用lower过滤器
void apply_lower(char *line) {
    for (int i = 0; line[i]; i++) {
        line[i] = tolower(line[i]);
    }
}

// 应用reverse过滤器
void apply_reverse(char *line) {
    int len = strlen(line);
    for (int i = 0; i < len / 2; i++) {
        char temp = line[i];
        line[i] = line[len - 1 - i];
        line[len - 1 - i] = temp;
    }
}

// 应用tr过滤器
void apply_tr(char *line, const char *from, const char *to) {
    // 简单实现：单字符替换
    for (int i = 0; line[i]; i++) {
        const char *pos = strchr(from, line[i]);
        if (pos && (pos - from) < strlen(to)) {
            line[i] = to[pos - from];
        }
    }
}

// 应用sed替换过滤器
void apply_sed(char *line, const char *pattern) {
    // 简单的s/old/new/替换
    if (strncmp(pattern, "s/", 2) == 0) {
        char *end = strrchr(pattern + 2, '/');
        if (end) {
            char old[256], new[256];
            char *first_slash = strchr(pattern + 2, '/');
            if (first_slash) {
                int old_len = first_slash - (pattern + 2);
                strncpy(old, pattern + 2, old_len);
                old[old_len] = '\0';
                
                int new_len = end - (first_slash + 1);
                strncpy(new, first_slash + 1, new_len);
                new[new_len] = '\0';
                
                // 简单替换（只替换第一个匹配）
                char *pos = strstr(line, old);
                if (pos) {
                    char buffer[MAX_LINE];
                    strncpy(buffer, line, pos - line);
                    buffer[pos - line] = '\0';
                    strcat(buffer, new);
                    strcat(buffer, pos + strlen(old));
                    strcpy(line, buffer);
                }
            }
        }
    }
}

// 应用cut过滤器
int apply_cut(char *line, const char *fields, const char *delimiter, char *result) {
    char *columns[MAX_COLUMNS];
    int col_count = 0;
    char line_copy[MAX_LINE];
    strcpy(line_copy, line);
    
    // 分割行
    char *token = strtok(line_copy, delimiter);
    while (token && col_count < MAX_COLUMNS) {
        columns[col_count++] = token;
        token = strtok(NULL, delimiter);
    }
    
    // 解析字段列表
    char fields_copy[256];
    strcpy(fields_copy, fields);
    int selected[MAX_COLUMNS] = {0};
    int select_count = 0;
    
    token = strtok(fields_copy, ",");
    while (token) {
        int field = atoi(token) - 1;  // 从1开始计数
        if (field >= 0 && field < col_count) {
            selected[field] = 1;
            select_count++;
        }
        token = strtok(NULL, ",");
    }
    
    if (select_count == 0) return 0;
    
    // 构建结果
    result[0] = '\0';
    for (int i = 0; i < col_count; i++) {
        if (selected[i]) {
            if (result[0] != '\0') {
                strcat(result, delimiter);
            }
            strcat(result, columns[i]);
        }
    }
    
    return 1;
}

// 应用join过滤器
void apply_join(char *line, const char *delimiter, const char *input) {
    // 简单实现：用分隔符连接当前行和输入
    if (line[0] != '\0') {
        strcat(line, delimiter);
    }
    strcat(line, input);
}

// 应用split过滤器
void apply_split(const char *line, const char *delimiter, char ***parts, int *count) {
    char line_copy[MAX_LINE];
    strcpy(line_copy, line);
    *count = 0;
    
    char *token = strtok(line_copy, delimiter);
    while (token && *count < MAX_COLUMNS) {
        (*parts)[*count] = strdup(token);
        (*count)++;
        token = strtok(NULL, delimiter);
    }
}

// 比较函数用于排序
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

int compare_strings_numeric(const void *a, const void *b) {
    double num_a = atof(*(const char **)a);
    double num_b = atof(*(const char **)b);
    if (num_a < num_b) return -1;
    if (num_a > num_b) return 1;
    return 0;
}

// 主处理函数
void process_stream(Config *config, FILE *input, FILE *output) {
    char buffer[BUFFER_SIZE];
    char line[MAX_LINE];
    int line_count = 0;
    int output_count = 0;
    clock_t start_time = clock();
    
    // 用于存储所有行（用于需要全部数据的操作如sort）
    char **all_lines = NULL;
    int all_lines_count = 0;
    int max_lines = 0;
    
    // 确定是否需要读取所有行
    int need_all_lines = 0;
    for (int i = 0; i < config->filter_count; i++) {
        if (config->filters[i].type == FILTER_SORT || 
            config->filters[i].type == FILTER_UNIQ ||
            config->filters[i].type == FILTER_TAIL ||
            config->filters[i].type == FILTER_REVERSE ||
            config->filters[i].type == FILTER_COUNT) {
            need_all_lines = 1;
            break;
        }
    }
    
    if (need_all_lines) {
        max_lines = 10000;  // 初始容量
        all_lines = malloc(max_lines * sizeof(char *));
    }
    
    // 跳过标题行
    if (config->skip_header && fgets(line, sizeof(line), input)) {
        line_count++;
    }
    
    // 读取和处理数据
    while (fgets(line, sizeof(line), input)) {
        line_count++;
        line[strcspn(line, "\n")] = '\0';  // 移除换行符
        
        char current_line[MAX_LINE];
        strcpy(current_line, line);
        
        // 应用所有过滤器
        int keep_line = 1;
        for (int i = 0; i < config->filter_count && keep_line; i++) {
            Filter *filter = &config->filters[i];
            
            switch (filter->type) {
                case FILTER_GREP: {
                    // 简单的grep实现
                    int found = 0;
                    if (filter->ignore_case) {
                        char line_lower[MAX_LINE], pattern_lower[MAX_LINE];
                        strcpy(line_lower, current_line);
                        strcpy(pattern_lower, filter->pattern);
                        apply_lower(line_lower);
                        apply_lower(pattern_lower);
                        found = (strstr(line_lower, pattern_lower) != NULL);
                    } else {
                        found = (strstr(current_line, filter->pattern) != NULL);
                    }
                    
                    if (filter->reverse) {
                        keep_line = !found;
                    } else {
                        keep_line = found;
                    }
                    break;
                }
                
                case FILTER_SED:
                    apply_sed(current_line, filter->pattern);
                    break;
                    
                case FILTER_CUT: {
                    char result[MAX_LINE];
                    if (apply_cut(current_line, filter->pattern, config->delimiter, result)) {
                        strcpy(current_line, result);
                    } else {
                        keep_line = 0;
                    }
                    break;
                }
                
                case FILTER_TRIM:
                    apply_trim(current_line);
                    break;
                    
                case FILTER_UPPER:
                    apply_upper(current_line);
                    break;
                    
                case FILTER_LOWER:
                    apply_lower(current_line);
                    break;
                    
                case FILTER_TR:
                    apply_tr(current_line, filter->pattern, filter->delimiter);
                    break;
                    
                case FILTER_JOIN:
                    // 需要特殊处理
                    break;
                    
                case FILTER_SPLIT:
                    // 需要特殊处理
                    break;
                    
                default:
                    // 其他过滤器需要所有数据
                    break;
            }
        }
        
        if (keep_line) {
            if (need_all_lines) {
                if (all_lines_count >= max_lines) {
                    max_lines *= 2;
                    all_lines = realloc(all_lines, max_lines * sizeof(char *));
                }
                all_lines[all_lines_count] = strdup(current_line);
                all_lines_count++;
            } else {
                fprintf(output, "%s\n", current_line);
                output_count++;
            }
        }
        
        // 显示进度
        if (config->verbose && line_count % 1000 == 0) {
            printf("已处理: %d 行\r", line_count);
            fflush(stdout);
        }
    }
    
    // 处理需要所有数据的过滤器
    if (need_all_lines && all_lines_count > 0) {
        // 按顺序应用需要所有数据的过滤器
        for (int i = 0; i < config->filter_count; i++) {
            Filter *filter = &config->filters[i];
            
            if (filter->type == FILTER_SORT) {
                if (filter->numeric) {
                    qsort(all_lines, all_lines_count, sizeof(char *), compare_strings_numeric);
                } else {
                    qsort(all_lines, all_lines_count, sizeof(char *), compare_strings);
                }
                
                if (filter->reverse) {
                    // 反转数组
                    for (int j = 0; j < all_lines_count / 2; j++) {
                        char *temp = all_lines[j];
                        all_lines[j] = all_lines[all_lines_count - 1 - j];
                        all_lines[all_lines_count - 1 - j] = temp;
                    }
                }
            }
            else if (filter->type == FILTER_UNIQ) {
                // 去重
                int unique_count = 0;
                for (int j = 0; j < all_lines_count; j++) {
                    int is_duplicate = 0;
                    for (int k = 0; k < unique_count; k++) {
                        if (strcmp(all_lines[j], all_lines[k]) == 0) {
                            is_duplicate = 1;
                            if (filter->numeric) {
                                // 计数模式
                                char count_str[20];
                                sprintf(count_str, "%d", atoi(all_lines[k]) + 1);
                                free(all_lines[k]);
                                all_lines[k] = strdup(count_str);
                            }
                            break;
                        }
                    }
                    
                    if (!is_duplicate) {
                        if (filter->numeric) {
                            // 计数模式，初始为1
                            char count_str[20];
                            sprintf(count_str, "1 %s", all_lines[j]);
                            free(all_lines[j]);
                            all_lines[j] = strdup(count_str);
                        }
                        if (unique_count != j) {
                            all_lines[unique_count] = all_lines[j];
                        }
                        unique_count++;
                    } else {
                        free(all_lines[j]);
                    }
                }
                all_lines_count = unique_count;
            }
            else if (filter->type == FILTER_TAIL) {
                int n = filter->end;
                if (n > 0 && n < all_lines_count) {
                    // 释放前面的行
                    for (int j = 0; j < all_lines_count - n; j++) {
                        free(all_lines[j]);
                    }
                    // 移动指针
                    for (int j = all_lines_count - n, k = 0; j < all_lines_count; j++, k++) {
                        all_lines[k] = all_lines[j];
                    }
                    all_lines_count = n;
                }
            }
            else if (filter->type == FILTER_HEAD) {
                int n = filter->end;
                if (n > 0 && n < all_lines_count) {
                    // 释放多余的行
                    for (int j = n; j < all_lines_count; j++) {
                        free(all_lines[j]);
                    }
                    all_lines_count = n;
                }
            }
            else if (filter->type == FILTER_REVERSE) {
                for (int j = 0; j < all_lines_count / 2; j++) {
                    char *temp = all_lines[j];
                    all_lines[j] = all_lines[all_lines_count - 1 - j];
                    all_lines[all_lines_count - 1 - j] = temp;
                }
            }
            else if (filter->type == FILTER_COUNT) {
                // 统计每行出现的次数
                // 这里简化处理，只显示行数
                // 实际应该统计每行的出现次数
            }
            else if (filter->type == FILTER_WC) {
                // 统计信息将在后面显示
            }
        }
        
        // 输出结果
        for (int j = 0; j < all_lines_count; j++) {
            fprintf(output, "%s\n", all_lines[j]);
            free(all_lines[j]);
            output_count++;
        }
        
        free(all_lines);
    }
    
    clock_t end_time = clock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    // 显示统计信息
    if (config->show_stats || config->verbose) {
        printf("\n\n");
        color_println(COLOR_CYAN, "处理统计:");
        printf("输入行数: %d\n", line_count);
        printf("输出行数: %d\n", output_count);
        printf("过滤比例: %.1f%%\n", line_count > 0 ? (float)output_count * 100 / line_count : 0);
        printf("处理时间: %.3f 秒\n", elapsed);
        printf("处理速度: %.0f 行/秒\n", elapsed > 0 ? line_count / elapsed : 0);
        
        // 如果使用了wc过滤器
        for (int i = 0; i < config->filter_count; i++) {
            if (config->filters[i].type == FILTER_WC) {
                printf("\n字数统计:\n");
                printf("  行数: %d\n", output_count);
                // 这里可以添加词数和字符数统计
            }
        }
    }
}

int main(int argc, char *argv[]) {
    Config config = {
        .filter_count = 0,
        .input_file = "",
        .output_file = "",
        .verbose = 0,
        .show_stats = 0,
        .interactive = 0,
        .batch_size = 1000,
        .delimiter = " ",
        .skip_header = 0,
        .column_output = 0
    };
    
    // 解析选项
    int opt;
    while ((opt = getopt(argc, argv, "i:o:d:Hb:vsh")) != -1) {
        switch (opt) {
            case 'i':
                strncpy(config.input_file, optarg, sizeof(config.input_file) - 1);
                break;
            case 'o':
                strncpy(config.output_file, optarg, sizeof(config.output_file) - 1);
                break;
            case 'd':
                strncpy(config.delimiter, optarg, sizeof(config.delimiter) - 1);
                break;
            case 'H':
                config.skip_header = 1;
                break;
            case 'b':
                config.batch_size = atoi(optarg);
                if (config.batch_size < 1) config.batch_size = 1000;
                break;
            case 'v':
                config.verbose = 1;
                break;
            case 's':
                config.show_stats = 1;
                break;
            case 'h':
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }
    
    // 解析过滤器
    int arg_index = optind;
    while (arg_index < argc && config.filter_count < MAX_FILTERS) {
        if (parse_filter(argc, argv, &arg_index, &config.filters[config.filter_count])) {
            config.filter_count++;
        } else {
            // 如果不是过滤器命令，可能是文件名
            if (strlen(config.input_file) == 0) {
                strncpy(config.input_file, argv[arg_index], sizeof(config.input_file) - 1);
            }
            arg_index++;
        }
    }
    
    if (config.verbose) {
        printf("配置:\n");
        printf("  输入文件: %s\n", strlen(config.input_file) > 0 ? config.input_file : "(标准输入)");
        printf("  输出文件: %s\n", strlen(config.output_file) > 0 ? config.output_file : "(标准输出)");
        printf("  分隔符: '%s'\n", config.delimiter);
        printf("  过滤器数: %d\n", config.filter_count);
    }
    
    // 打开输入文件
    FILE *input = stdin;
    if (strlen(config.input_file) > 0) {
        input = fopen(config.input_file, "r");
        if (!input) {
            print_error("无法打开输入文件: %s", config.input_file);
            return 1;
        }
    }
    
    // 打开输出文件
    FILE *output = stdout;
    if (strlen(config.output_file) > 0) {
        output = fopen(config.output_file, "w");
        if (!output) {
            print_error("无法创建输出文件: %s", config.output_file);
            if (input != stdin) fclose(input);
            return 1;
        }
    }
    
    // 处理流
    process_stream(&config, input, output);
    
    // 清理
    if (input != stdin) fclose(input);
    if (output != stdout) fclose(output);
    
    return 0;
}