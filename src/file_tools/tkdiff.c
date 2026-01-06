// src/file_tools/tkdiff.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 差异类型
typedef enum {
    DIFF_EQUAL,     // 相同
    DIFF_INSERT,    // 插入
    DIFF_DELETE,    // 删除
    DIFF_CHANGE     // 修改
} DiffType;

// 差异块
typedef struct DiffChunk {
    DiffType type;
    int file1_start;
    int file1_end;
    int file2_start;
    int file2_end;
    char **lines1;  // 文件1的行
    char **lines2;  // 文件2的行
    int line_count1;
    int line_count2;
    struct DiffChunk *next;
} DiffChunk;

// 文件信息
typedef struct {
    char *filename;
    char **lines;
    int line_count;
    char *content;
    size_t size;
    time_t mtime;
} FileInfo;

// 选项
typedef struct {
    int color_output;       // 彩色输出
    int context_lines;      // 上下文行数
    int unified_diff;       // 统一差异格式
    int side_by_side;       // 并排显示
    int ignore_case;        // 忽略大小写
    int ignore_whitespace;  // 忽略空白
    int show_stats;         // 显示统计
    int brief;              // 简要输出
    int recursive;          // 递归比较目录
    int help;               // 帮助
    int version;            // 版本
} Options;

// 初始化选项
static void init_options(Options *opts) {
    opts->color_output = is_color_supported();
    opts->context_lines = 3;
    opts->unified_diff = 1;  // 默认统一格式
    opts->side_by_side = 0;
    opts->ignore_case = 0;
    opts->ignore_whitespace = 0;
    opts->show_stats = 0;
    opts->brief = 0;
    opts->recursive = 0;
    opts->help = 0;
    opts->version = 0;
}

// 显示帮助
static void show_help() {
    printf("tkdiff - 文件比较和合并工具\n");
    printf("用法: tkdiff [选项] 文件1 文件2\n");
    printf("选项:\n");
    printf("  -c, --context NUM  显示NUM行上下文（默认: 3）\n");
    printf("  -u, --unified      统一差异格式（默认）\n");
    printf("  -y, --side-by-side 并排显示\n");
    printf("  -i, --ignore-case  忽略大小写\n");
    printf("  -w, --ignore-all-space 忽略所有空白\n");
    printf("  -s, --stats        显示统计信息\n");
    printf("  -q, --brief        简要输出（仅报告文件是否不同）\n");
    printf("  -r, --recursive    递归比较目录\n");
    printf("      --no-color     无颜色输出\n");
    printf("      --help         显示帮助\n");
    printf("      --version      显示版本\n");
}

// 显示版本
static void show_version() {
    printf("tkdiff v1.0.0 - TermKit 文件比较工具\n");
}

// 解析选项
static int parse_options(int argc, char **argv, Options *opts, char **file1, char **file2) {
    *file1 = NULL;
    *file2 = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--context") == 0) {
            if (i + 1 < argc) opts->context_lines = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--unified") == 0) {
            opts->unified_diff = 1;
            opts->side_by_side = 0;
        } else if (strcmp(argv[i], "-y") == 0 || strcmp(argv[i], "--side-by-side") == 0) {
            opts->side_by_side = 1;
            opts->unified_diff = 0;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-case") == 0) {
            opts->ignore_case = 1;
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--ignore-all-space") == 0) {
            opts->ignore_whitespace = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0) {
            opts->show_stats = 1;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--brief") == 0) {
            opts->brief = 1;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
            opts->recursive = 1;
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
            if (*file1 == NULL) {
                *file1 = argv[i];
            } else if (*file2 == NULL) {
                *file2 = argv[i];
            } else {
                print_error("多余的参数: %s", argv[i]);
                return -1;
            }
        }
    }
    
    if (*file1 == NULL || *file2 == NULL) {
        print_error("需要两个文件参数");
        return -1;
    }
    
    return 1;
}

// 读取文件内容
static int read_file(const char *filename, FileInfo *file) {
    file->filename = strdup(filename);
    file->lines = NULL;
    file->line_count = 0;
    file->content = NULL;
    file->size = 0;
    
    // 获取文件信息
    struct stat st;
    if (stat(filename, &st) == -1) {
        return 0;
    }
    file->mtime = st.st_mtime;
    
    // 读取文件内容
    if (!file_read_all(filename, &file->content, &file->size)) {
        return 0;
    }
    
    // 分割为行
    char *content = file->content;
    char *line;
    int capacity = 16;
    
    file->lines = malloc(sizeof(char*) * capacity);
    
    while ((line = strsep(&content, "\n")) != NULL) {
        if (file->line_count >= capacity) {
            capacity *= 2;
            file->lines = realloc(file->lines, sizeof(char*) * capacity);
        }
        
        file->lines[file->line_count] = strdup(line);
        file->line_count++;
    }
    
    return 1;
}

// 清理文件信息
static void free_file_info(FileInfo *file) {
    if (file->filename) free(file->filename);
    if (file->content) free(file->content);
    
    if (file->lines) {
        for (int i = 0; i < file->line_count; i++) {
            free(file->lines[i]);
        }
        free(file->lines);
    }
}

// 预处理行（根据选项）
static char* preprocess_line(const char *line, Options *opts) {
    if (!line) return strdup("");
    
    char *processed = strdup(line);
    
    if (opts->ignore_whitespace) {
        // 移除所有空白字符
        char *dst = processed;
        const char *src = processed;
        
        while (*src) {
            if (!isspace((unsigned char)*src)) {
                *dst++ = *src;
            }
            src++;
        }
        *dst = '\0';
    }
    
    if (opts->ignore_case) {
        // 转换为小写
        for (char *p = processed; *p; p++) {
            *p = tolower((unsigned char)*p);
        }
    }
    
    return processed;
}

// 比较两行
static int compare_lines(const char *line1, const char *line2, Options *opts) {
    if (opts->ignore_whitespace || opts->ignore_case) {
        char *proc1 = preprocess_line(line1, opts);
        char *proc2 = preprocess_line(line2, opts);
        int result = strcmp(proc1, proc2);
        free(proc1);
        free(proc2);
        return result == 0;
    } else {
        return strcmp(line1, line2) == 0;
    }
}

// 查找最长公共子序列（简化版）
static int** compute_lcs(FileInfo *file1, FileInfo *file2, Options *opts) {
    int m = file1->line_count;
    int n = file2->line_count;
    
    // 分配二维数组
    int **lcs = malloc((m + 1) * sizeof(int*));
    for (int i = 0; i <= m; i++) {
        lcs[i] = malloc((n + 1) * sizeof(int));
        for (int j = 0; j <= n; j++) {
            lcs[i][j] = 0;
        }
    }
    
    // 计算LCS
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (compare_lines(file1->lines[i-1], file2->lines[j-1], opts)) {
                lcs[i][j] = lcs[i-1][j-1] + 1;
            } else {
                lcs[i][j] = (lcs[i-1][j] > lcs[i][j-1]) ? lcs[i-1][j] : lcs[i][j-1];
            }
        }
    }
    
    return lcs;
}

// 提取差异
static DiffChunk* extract_diff(FileInfo *file1, FileInfo *file2, int **lcs, Options *opts) {
    int i = file1->line_count;
    int j = file2->line_count;
    DiffChunk *head = NULL;
    DiffChunk *tail = NULL;
    
    while (i > 0 || j > 0) {
        DiffChunk *chunk = malloc(sizeof(DiffChunk));
        chunk->next = NULL;
        
        if (i > 0 && j > 0 && compare_lines(file1->lines[i-1], file2->lines[j-1], opts)) {
            // 行相同
            chunk->type = DIFF_EQUAL;
            chunk->file1_start = chunk->file1_end = i;
            chunk->file2_start = chunk->file2_end = j;
            chunk->lines1 = malloc(sizeof(char*));
            chunk->lines1[0] = strdup(file1->lines[i-1]);
            chunk->line_count1 = 1;
            chunk->lines2 = NULL;
            chunk->line_count2 = 0;
            i--;
            j--;
        } else if (j > 0 && (i == 0 || lcs[i][j-1] >= lcs[i-1][j])) {
            // 文件2有插入
            chunk->type = DIFF_INSERT;
            chunk->file1_start = chunk->file1_end = i;
            chunk->file2_start = chunk->file2_end = j;
            chunk->lines1 = NULL;
            chunk->line_count1 = 0;
            chunk->lines2 = malloc(sizeof(char*));
            chunk->lines2[0] = strdup(file2->lines[j-1]);
            chunk->line_count2 = 1;
            j--;
        } else if (i > 0 && (j == 0 || lcs[i][j-1] < lcs[i-1][j])) {
            // 文件1有删除
            chunk->type = DIFF_DELETE;
            chunk->file1_start = chunk->file1_end = i;
            chunk->file2_start = chunk->file2_end = j;
            chunk->lines1 = malloc(sizeof(char*));
            chunk->lines1[0] = strdup(file1->lines[i-1]);
            chunk->line_count1 = 1;
            chunk->lines2 = NULL;
            chunk->line_count2 = 0;
            i--;
        }
        
        // 合并相邻的相同类型块
        if (tail && tail->type == chunk->type) {
            if (chunk->type == DIFF_EQUAL || chunk->type == DIFF_INSERT || chunk->type == DIFF_DELETE) {
                // 扩展前一个块
                if (chunk->type == DIFF_EQUAL) {
                    tail->file1_start = chunk->file1_start;
                    tail->file2_start = chunk->file2_start;
                    
                    char **new_lines = malloc(sizeof(char*) * (tail->line_count1 + 1));
                    for (int k = 0; k < tail->line_count1; k++) {
                        new_lines[k] = tail->lines1[k];
                    }
                    new_lines[tail->line_count1] = chunk->lines1[0];
                    free(tail->lines1);
                    tail->lines1 = new_lines;
                    tail->line_count1++;
                } else if (chunk->type == DIFF_INSERT) {
                    tail->file2_start = chunk->file2_start;
                    
                    char **new_lines = malloc(sizeof(char*) * (tail->line_count2 + 1));
                    for (int k = 0; k < tail->line_count2; k++) {
                        new_lines[k] = tail->lines2[k];
                    }
                    new_lines[tail->line_count2] = chunk->lines2[0];
                    free(tail->lines2);
                    tail->lines2 = new_lines;
                    tail->line_count2++;
                } else if (chunk->type == DIFF_DELETE) {
                    tail->file1_start = chunk->file1_start;
                    
                    char **new_lines = malloc(sizeof(char*) * (tail->line_count1 + 1));
                    for (int k = 0; k < tail->line_count1; k++) {
                        new_lines[k] = tail->lines1[k];
                    }
                    new_lines[tail->line_count1] = chunk->lines1[0];
                    free(tail->lines1);
                    tail->lines1 = new_lines;
                    tail->line_count1++;
                }
                
                free(chunk->lines1);
                free(chunk->lines2);
                free(chunk);
                continue;
            }
        }
        
        // 添加到链表
        if (head == NULL) {
            head = tail = chunk;
        } else {
            chunk->next = head;
            head = chunk;
        }
    }
    
    return head;
}

// 释放差异块
static void free_diff_chunks(DiffChunk *head) {
    while (head) {
        DiffChunk *next = head->next;
        
        if (head->lines1) {
            for (int i = 0; i < head->line_count1; i++) {
                free(head->lines1[i]);
            }
            free(head->lines1);
        }
        
        if (head->lines2) {
            for (int i = 0; i < head->line_count2; i++) {
                free(head->lines2[i]);
            }
            free(head->lines2);
        }
        
        free(head);
        head = next;
    }
}

// 显示统一格式差异
static void show_unified_diff(FileInfo *file1, FileInfo *file2, 
                             DiffChunk *diff, Options *opts) {
    // 文件头
    if (opts->color_output) {
        color_print(COLOR_BRIGHT_CYAN, "--- ");
        printf("%s\t%s", file1->filename, format_time(file1->mtime));
        printf("\n");
        
        color_print(COLOR_BRIGHT_CYAN, "+++ ");
        printf("%s\t%s", file2->filename, format_time(file2->mtime));
        printf("\n");
    } else {
        printf("--- %s\t%s\n", file1->filename, format_time(file1->mtime));
        printf("+++ %s\t%s\n", file2->filename, format_time(file2->mtime));
    }
    
    DiffChunk *current = diff;
    while (current) {
        if (current->type == DIFF_EQUAL) {
            // 跳过相同的行
            current = current->next;
            continue;
        }
        
        // 块头
        int start1 = current->file1_start;
        int count1 = current->line_count1;
        int start2 = current->file2_start;
        int count2 = current->line_count2;
        
        if (opts->color_output) {
            color_print(COLOR_BRIGHT_MAGENTA, "@@ -%d,%d +%d,%d @@\n", 
                       start1, count1, start2, count2);
        } else {
            printf("@@ -%d,%d +%d,%d @@\n", start1, count1, start2, count2);
        }
        
        // 显示行
        int i = 0, j = 0;
        while (i < count1 || j < count2) {
            char prefix = ' ';
            const char *color = NULL;
            const char *line = NULL;
            
            if (i < count1 && j < count2) {
                // 修改
                prefix = '!';
                color = COLOR_BRIGHT_YELLOW;
                line = current->lines1[i];
                i++;
            } else if (i < count1) {
                // 删除
                prefix = '-';
                color = COLOR_BRIGHT_RED;
                line = current->lines1[i];
                i++;
            } else if (j < count2) {
                // 插入
                prefix = '+';
                color = COLOR_BRIGHT_GREEN;
                line = current->lines2[j];
                j++;
            }
            
            if (opts->color_output && color) {
                printf("%s%c %s%s\n", color, prefix, line, COLOR_RESET);
            } else {
                printf("%c %s\n", prefix, line);
            }
        }
        
        current = current->next;
    }
}

// 显示并排差异
static void show_side_by_side_diff(FileInfo *file1, FileInfo *file2, 
                                  DiffChunk *diff, Options *opts) {
    int width = 40;
    
    // 标题
    if (opts->color_output) {
        color_print(COLOR_BRIGHT_CYAN, "%-*s | %-*s\n", width, file1->filename, width, file2->filename);
        for (int i = 0; i < width * 2 + 3; i++) printf("═");
        printf("\n");
    } else {
        printf("%-*s | %-*s\n", width, file1->filename, width, file2->filename);
        for (int i = 0; i < width * 2 + 3; i++) printf("=");
        printf("\n");
    }
    
    DiffChunk *current = diff;
    int line1 = 1, line2 = 1;
    
    while (current) {
        if (current->type == DIFF_EQUAL) {
            // 显示相同的行
            for (int i = 0; i < current->line_count1; i++) {
                char line1_str[width + 1];
                char line2_str[width + 1];
                
                // 截断过长的行
                strncpy(line1_str, current->lines1[i], width);
                line1_str[width] = '\0';
                strncpy(line2_str, current->lines1[i], width);
                line2_str[width] = '\0';
                
                if (opts->color_output) {
                    printf("%s%-*s %s| %-*s%s\n", 
                           COLOR_BRIGHT_BLUE, width, line1_str,
                           COLOR_RESET, width, line2_str, COLOR_RESET);
                } else {
                    printf("%-*s | %-*s\n", width, line1_str, width, line2_str);
                }
                
                line1++;
                line2++;
            }
        } else if (current->type == DIFF_DELETE) {
            // 只显示在文件1中
            for (int i = 0; i < current->line_count1; i++) {
                char line_str[width + 1];
                strncpy(line_str, current->lines1[i], width);
                line_str[width] = '\0';
                
                if (opts->color_output) {
                    printf("%s%-*s %s| %-*s%s\n", 
                           COLOR_BRIGHT_RED, width, line_str,
                           COLOR_RESET, width, "", COLOR_RESET);
                } else {
                    printf("%-*s | %-*s\n", width, line_str, width, "");
                }
                
                line1++;
            }
        } else if (current->type == DIFF_INSERT) {
            // 只显示在文件2中
            for (int i = 0; i < current->line_count2; i++) {
                char line_str[width + 1];
                strncpy(line_str, current->lines2[i], width);
                line_str[width] = '\0';
                
                if (opts->color_output) {
                    printf("%s%-*s %s| %s%-*s%s\n", 
                           COLOR_RESET, width, "",
                           COLOR_RESET,
                           COLOR_BRIGHT_GREEN, width, line_str, COLOR_RESET);
                } else {
                    printf("%-*s | %-*s\n", width, "", width, line_str);
                }
                
                line2++;
            }
        }
        
        current = current->next;
    }
}

// 计算统计信息
static void compute_stats(DiffChunk *diff, int *inserts, int *deletes, int *changes) {
    *inserts = *deletes = *changes = 0;
    DiffChunk *current = diff;
    
    while (current) {
        if (current->type == DIFF_INSERT) {
            *inserts += current->line_count2;
        } else if (current->type == DIFF_DELETE) {
            *deletes += current->line_count1;
        } else if (current->type == DIFF_CHANGE) {
            *changes += (current->line_count1 + current->line_count2) / 2;
        }
        current = current->next;
    }
}

// 显示统计信息
static void show_stats(FileInfo *file1, FileInfo *file2, DiffChunk *diff, Options *opts) {
    int inserts, deletes, changes;
    compute_stats(diff, &inserts, &deletes, &changes);
    
    int total_changes = inserts + deletes + changes;
    
    if (opts->color_output) {
        color_println(COLOR_BRIGHT_CYAN, "📊 差异统计:");
        printf("══════════════════════════════════════════════════════════════\n");
        
        color_print(COLOR_BRIGHT_GREEN, "文件1: ");
        printf("%s (%d 行)\n", file1->filename, file1->line_count);
        
        color_print(COLOR_BRIGHT_GREEN, "文件2: ");
        printf("%s (%d 行)\n", file2->filename, file2->line_count);
        
        printf("\n");
        
        if (total_changes == 0) {
            color_println(COLOR_BRIGHT_GREEN, "✅ 文件完全相同");
        } else {
            if (inserts > 0) {
                color_print(COLOR_BRIGHT_GREEN, "➕ 插入: ");
                printf("%d 行\n", inserts);
            }
            
            if (deletes > 0) {
                color_print(COLOR_BRIGHT_RED, "➖ 删除: ");
                printf("%d 行\n", deletes);
            }
            
            if (changes > 0) {
                color_print(COLOR_BRIGHT_YELLOW, "✏️  修改: ");
                printf("%d 处\n", changes);
            }
            
            printf("\n");
            color_print(COLOR_BRIGHT_CYAN, "📈 总差异: ");
            printf("%d 处修改\n", total_changes);
        }
    } else {
        printf("差异统计:\n");
        printf("══════════════════════════════════════════════════════════════\n");
        
        printf("文件1: %s (%d 行)\n", file1->filename, file1->line_count);
        printf("文件2: %s (%d 行)\n", file2->filename, file2->line_count);
        
        printf("\n");
        
        if (total_changes == 0) {
            printf("✅ 文件完全相同\n");
        } else {
            if (inserts > 0) printf("➕ 插入: %d 行\n", inserts);
            if (deletes > 0) printf("➖ 删除: %d 行\n", deletes);
            if (changes > 0) printf("✏️  修改: %d 处\n", changes);
            
            printf("\n📈 总差异: %d 处修改\n", total_changes);
        }
    }
    
    printf("\n");
}

// 简要输出
static void show_brief(FileInfo *file1, FileInfo *file2, DiffChunk *diff, Options *opts) {
    int inserts, deletes, changes;
    compute_stats(diff, &inserts, &deletes, &changes);
    
    if (inserts == 0 && deletes == 0 && changes == 0) {
        if (opts->color_output) {
            color_println(COLOR_BRIGHT_GREEN, "文件 %s 和 %s 相同", file1->filename, file2->filename);
        } else {
            printf("文件 %s 和 %s 相同\n", file1->filename, file2->filename);
        }
    } else {
        if (opts->color_output) {
            color_println(COLOR_BRIGHT_YELLOW, "文件 %s 和 %s 不同", file1->filename, file2->filename);
        } else {
            printf("文件 %s 和 %s 不同\n", file1->filename, file2->filename);
        }
    }
}

// 比较两个文件
static int compare_files(const char *file1_path, const char *file2_path, Options *opts) {
    FileInfo file1, file2;
    
    if (!read_file(file1_path, &file1)) {
        print_error("无法读取文件: %s", file1_path);
        return 1;
    }
    
    if (!read_file(file2_path, &file2)) {
        print_error("无法读取文件: %s", file2_path);
        free_file_info(&file1);
        return 1;
    }
    
    // 计算LCS
    int **lcs = compute_lcs(&file1, &file2, opts);
    
    // 提取差异
    DiffChunk *diff = extract_diff(&file1, &file2, lcs, opts);
    
    // 显示结果
    if (opts->brief) {
        show_brief(&file1, &file2, diff, opts);
    } else if (opts->show_stats) {
        show_stats(&file1, &file2, diff, opts);
    } else if (opts->side_by_side) {
        show_side_by_side_diff(&file1, &file2, diff, opts);
    } else {
        show_unified_diff(&file1, &file2, diff, opts);
    }
    
    // 清理
    for (int i = 0; i <= file1.line_count; i++) {
        free(lcs[i]);
    }
    free(lcs);
    free_diff_chunks(diff);
    free_file_info(&file1);
    free_file_info(&file2);
    
    return 0;
}

// 比较两个目录
static int compare_directories(const char *dir1_path, const char *dir2_path, Options *opts) {
    // 简化版：只报告目录是否相同
    struct stat st1, st2;
    
    if (stat(dir1_path, &st1) == -1) {
        print_error("无法访问目录: %s", dir1_path);
        return 1;
    }
    
    if (stat(dir2_path, &st2) == -1) {
        print_error("无法访问目录: %s", dir2_path);
        return 1;
    }
    
    if (!S_ISDIR(st1.st_mode) || !S_ISDIR(st2.st_mode)) {
        print_error("两个参数都必须是目录");
        return 1;
    }
    
    if (opts->color_output) {
        color_println(COLOR_BRIGHT_CYAN, "比较目录: %s 和 %s", dir1_path, dir2_path);
    } else {
        printf("比较目录: %s 和 %s\n", dir1_path, dir2_path);
    }
    
    // 这里可以添加递归比较目录内容的代码
    printf("（目录比较功能待完善）\n");
    
    return 0;
}

// tkdiff主函数
int tkdiff_main(int argc, char **argv) {
    Options opts;
    init_options(&opts);
    
    char *file1 = NULL;
    char *file2 = NULL;
    
    int parse_result = parse_options(argc, argv, &opts, &file1, &file2);
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
    
    // 检查文件是否存在
    if (!file_exists(file1)) {
        print_error("文件不存在: %s", file1);
        return 1;
    }
    
    if (!file_exists(file2)) {
        print_error("文件不存在: %s", file2);
        return 1;
    }
    
    // 判断是文件比较还是目录比较
    struct stat st1, st2;
    stat(file1, &st1);
    stat(file2, &st2);
    
    if (S_ISDIR(st1.st_mode) && S_ISDIR(st2.st_mode)) {
        // 目录比较
        return compare_directories(file1, file2, &opts);
    } else if (!S_ISDIR(st1.st_mode) && !S_ISDIR(st2.st_mode)) {
        // 文件比较
        return compare_files(file1, file2, &opts);
    } else {
        // 类型不同
        print_error("不能比较文件和目录");
        return 1;
    }
}