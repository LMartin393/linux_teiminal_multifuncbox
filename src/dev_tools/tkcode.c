// src/dev_tools/tkcode.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 文件类型结构
typedef struct {
    const char *extension;
    const char *name;
    const char *icon;
} FileType;

// 文件类型定义
static FileType file_types[] = {
    {".c",    "C Source",       "📝"},
    {".cpp",  "C++ Source",     "📝"},
    {".cc",   "C++ Source",     "📝"},
    {".h",    "C Header",       "📋"},
    {".hpp",  "C++ Header",     "📋"},
    {".py",   "Python",         "🐍"},
    {".java", "Java",           "☕"},
    {".js",   "JavaScript",     "📜"},
    {".ts",   "TypeScript",     "📘"},
    {".html", "HTML",           "🌐"},
    {".css",  "CSS",            "🎨"},
    {".php",  "PHP",            "🐘"},
    {".rb",   "Ruby",           "💎"},
    {".go",   "Go",             "🐹"},
    {".rs",   "Rust",           "🦀"},
    {".swift","Swift",          "🐦"},
    {".kt",   "Kotlin",         "🅺"},
    {".sh",   "Shell Script",   "🐚"},
    {".pl",   "Perl",           "🐪"},
    {".lua",  "Lua",            "🌙"},
    {".sql",  "SQL",            "🗄️ "},
    {".json", "JSON",           "📋"},
    {".xml",  "XML",            "📄"},
    {".yml",  "YAML",           "⚙️ "},
    {".yaml", "YAML",           "⚙️ "},
    {".md",   "Markdown",       "📖"},
    {".txt",  "Text",           "📄"},
    {NULL,    "Other",          "📄"}
};

// 统计结果结构
typedef struct {
    char filename[256];
    int total_lines;
    int code_lines;
    int comment_lines;
    int blank_lines;
    int file_size;
    char language[32];
    const char *icon;
} FileStats;

// 目录统计
typedef struct {
    char language[32];
    int file_count;
    int total_lines;
    int code_lines;
    int comment_lines;
    int blank_lines;
    const char *icon;
} LanguageStats;

// 选项结构
typedef struct {
    int recursive;        // -r 递归统计
    int summary_only;     // -s 只显示汇总
    int by_language;      // -l 按语言分组
    int by_file;          // -f 显示每个文件统计
    int show_percentage;  // -p 显示百分比
    int show_icons;       // 显示图标
    int color;           // 彩色输出
    int sort_by_lines;    // 按行数排序
    int sort_by_files;    // 按文件数排序
    int exclude_patterns; // 排除模式（简化）
    int help;            // 帮助信息
    int version;         // 版本信息
    char **paths;        // 要统计的路径
    int path_count;      // 路径数量
} Options;

// 初始化选项
static void init_options(Options *opts) {
    opts->recursive = 0;
    opts->summary_only = 0;
    opts->by_language = 0;
    opts->by_file = 1; // 默认显示文件统计
    opts->show_percentage = 0;
    opts->show_icons = 1;
    opts->color = is_color_supported();
    opts->sort_by_lines = 0;
    opts->sort_by_files = 0;
    opts->exclude_patterns = 0;
    opts->help = 0;
    opts->version = 0;
    opts->paths = NULL;
    opts->path_count = 0;
}

// 显示帮助
static void show_help(void) {
    color_println(COLOR_BRIGHT_CYAN, "tkcode - 代码统计工具");
    printf("\n");
    printf("用法: tkcode [选项] [文件/目录]...\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "统计选项:");
    printf("  -r, --recursive      递归统计子目录\n");
    printf("  -s, --summary        只显示汇总统计\n");
    printf("  -l, --by-language    按编程语言分组统计\n");
    printf("  -f, --by-file        显示每个文件的统计（默认）\n");
    printf("  -p, --percentage     显示百分比\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "显示选项:");
    printf("      --no-icons       不显示图标\n");
    printf("      --color          彩色输出（默认）\n");
    printf("      --no-color       黑白输出\n");
    printf("      --sort-lines     按代码行数排序\n");
    printf("      --sort-files     按文件数排序\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "其他:");
    printf("      --help           显示此帮助\n");
    printf("      --version        显示版本\n");
    printf("\n");
    color_println(COLOR_BRIGHT_GREEN, "示例:");
    printf("  tkcode file.c                # 统计单个文件\n");
    printf("  tkcode *.c *.h              # 统计多个文件\n");
    printf("  tkcode src/                 # 统计目录\n");
    printf("  tkcode -r src/              # 递归统计\n");
    printf("  tkcode -l src/              # 按语言分组\n");
    printf("  tkcode -s -l project/       # 按语言汇总\n");
    printf("  tkcode --sort-lines src/    # 按行数排序\n");
}

// 显示版本
static void show_version(void) {
    color_println(COLOR_BRIGHT_MAGENTA, "tkcode - TermKit 代码统计工具");
    printf("版本: 1.0.0\n");
    printf("功能: 统计代码行数、注释、空行，支持多种语言\n");
    printf("支持语言: C, C++, Python, Java, JavaScript, Go, Rust 等\n");
}

// 解析选项
static int parse_options(int argc, char **argv, Options *opts) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
                opts->recursive = 1;
            } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--summary") == 0) {
                opts->summary_only = 1;
                opts->by_file = 0;
            } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--by-language") == 0) {
                opts->by_language = 1;
                opts->by_file = 0;
            } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--by-file") == 0) {
                opts->by_file = 1;
            } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--percentage") == 0) {
                opts->show_percentage = 1;
            } else if (strcmp(argv[i], "--no-icons") == 0) {
                opts->show_icons = 0;
            } else if (strcmp(argv[i], "--color") == 0) {
                opts->color = 1;
            } else if (strcmp(argv[i], "--no-color") == 0) {
                opts->color = 0;
            } else if (strcmp(argv[i], "--sort-lines") == 0) {
                opts->sort_by_lines = 1;
            } else if (strcmp(argv[i], "--sort-files") == 0) {
                opts->sort_by_files = 1;
            } else if (strcmp(argv[i], "--help") == 0) {
                opts->help = 1;
                return 1;
            } else if (strcmp(argv[i], "--version") == 0) {
                opts->version = 1;
                return 1;
            } else {
                print_error("无效选项: %s", argv[i]);
                printf("使用 'tkcode --help' 查看帮助\n");
                return -1;
            }
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

// 根据扩展名获取文件类型信息
static FileType* get_file_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return &file_types[sizeof(file_types)/sizeof(file_types[0]) - 1];
    }
    
    for (int i = 0; file_types[i].extension != NULL; i++) {
        if (strcasecmp(ext, file_types[i].extension) == 0) {
            return &file_types[i];
        }
    }
    
    return &file_types[sizeof(file_types)/sizeof(file_types[0]) - 1];
}

// 判断是否为代码文件
static int is_code_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) return 0;
    
    // 检查是否为支持的代码文件扩展名
    for (int i = 0; file_types[i].extension != NULL; i++) {
        if (strcasecmp(ext, file_types[i].extension) == 0) {
            return 1;
        }
    }
    
    return 0;
}

// 判断是否为注释（针对不同语言）
static int is_comment_line(const char *line, const char *language) {
    if (line == NULL || strlen(line) == 0) return 0;
    
    // 去除前导空格
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    
    // 空行
    if (*p == '\0') return 0;
    
    // C/C++/Java/JavaScript 等
    if (strstr(language, "C") != NULL || strstr(language, "Java") != NULL || 
        strstr(language, "JavaScript") != NULL || strstr(language, "Go") != NULL ||
        strstr(language, "Rust") != NULL) {
        if (strncmp(p, "//", 2) == 0) return 1;
        if (strncmp(p, "/*", 2) == 0) return 1;
        if (strncmp(p, "*", 1) == 0) return 1;
    }
    
    // Python/Shell/Perl/Ruby
    if (strstr(language, "Python") != NULL || strstr(language, "Shell") != NULL ||
        strstr(language, "Perl") != NULL || strstr(language, "Ruby") != NULL ||
        strstr(language, "YAML") != NULL) {
        if (*p == '#') return 1;
    }
    
    // HTML/XML
    if (strstr(language, "HTML") != NULL || strstr(language, "XML") != NULL) {
        if (strncmp(p, "<!--", 4) == 0) return 1;
        if (strstr(p, "-->") != NULL) return 1;
    }
    
    // SQL
    if (strstr(language, "SQL") != NULL) {
        if (*p == '-' && *(p+1) == '-') return 1;
    }
    
    // PHP
    if (strstr(language, "PHP") != NULL) {
        if (*p == '#') return 1;
        if (strncmp(p, "//", 2) == 0) return 1;
        if (strncmp(p, "/*", 2) == 0) return 1;
    }
    
    return 0;
}

// 统计单个文件
static int count_file_lines(const char *filename, FileStats *stats) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return 0;
    }
    
    // 获取文件类型
    FileType *type = get_file_type(filename);
    strncpy(stats->language, type->name, sizeof(stats->language) - 1);
    stats->icon = type->icon;
    
    char line[4096];
    int in_block_comment = 0;
    stats->total_lines = 0;
    stats->code_lines = 0;
    stats->comment_lines = 0;
    stats->blank_lines = 0;
    
    while (fgets(line, sizeof(line), file)) {
        stats->total_lines++;
        
        // 去除换行符
        line[strcspn(line, "\n")] = '\0';
        
        // 去除前导空格
        const char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        
        // 检查空行
        if (*p == '\0') {
            stats->blank_lines++;
            continue;
        }
        
        // 处理块注释
        if (in_block_comment) {
            stats->comment_lines++;
            // 检查块注释结束
            if (strstr(p, "*/") != NULL) {
                in_block_comment = 0;
            }
            continue;
        }
        
        // 检查块注释开始
        if (strncmp(p, "/*", 2) == 0) {
            stats->comment_lines++;
            in_block_comment = 1;
            // 检查同一行结束的块注释
            if (strstr(p, "*/") != NULL) {
                in_block_comment = 0;
            }
            continue;
        }
        
        // 检查行注释
        if (is_comment_line(p, stats->language)) {
            stats->comment_lines++;
            continue;
        }
        
        // 代码行
        stats->code_lines++;
    }
    
    fclose(file);
    
    // 获取文件大小
    struct stat st;
    if (stat(filename, &st) == 0) {
        stats->file_size = st.st_size;
    }
    
    return 1;
}

// 收集代码文件（递归）
static int collect_code_files(const char *path, Options *opts, 
                             FileStats **files, int *file_count) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        // 可能是文件而不是目录
        if (is_code_file(path)) {
            *file_count = 1;
            *files = malloc(sizeof(FileStats));
            strncpy((*files)[0].filename, path, sizeof((*files)[0].filename) - 1);
            return 1;
        }
        return 0;
    }
    
    struct dirent *entry;
    int capacity = 16;
    *files = malloc(sizeof(FileStats) * capacity);
    *file_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 跳过隐藏文件
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (entry->d_type == DT_DIR) {
            if (opts->recursive) {
                FileStats *sub_files;
                int sub_count;
                if (collect_code_files(full_path, opts, &sub_files, &sub_count)) {
                    // 扩展数组
                    while (*file_count + sub_count >= capacity) {
                        capacity *= 2;
                        *files = realloc(*files, sizeof(FileStats) * capacity);
                    }
                    
                    // 复制子文件
                    memcpy(*files + *file_count, sub_files, sizeof(FileStats) * sub_count);
                    *file_count += sub_count;
                    free(sub_files);
                }
            }
        } else if (entry->d_type == DT_REG) {
            if (is_code_file(entry->d_name)) {
                if (*file_count >= capacity) {
                    capacity *= 2;
                    *files = realloc(*files, sizeof(FileStats) * capacity);
                }
                
                strncpy((*files)[*file_count].filename, full_path, 
                       sizeof((*files)[*file_count].filename) - 1);
                (*file_count)[*files].filename[sizeof((*files)[*file_count].filename) - 1] = '\0';
                (*file_count)++;
            }
        }
    }
    
    closedir(dir);
    return *file_count > 0;
}

// 按语言汇总统计
static void aggregate_by_language(FileStats *files, int file_count, 
                                 LanguageStats **lang_stats, int *lang_count) {
    // 创建语言统计数组
    *lang_stats = NULL;
    *lang_count = 0;
    int capacity = 16;
    *lang_stats = malloc(sizeof(LanguageStats) * capacity);
    
    for (int i = 0; i < file_count; i++) {
        // 查找是否已存在该语言的统计
        int found = 0;
        for (int j = 0; j < *lang_count; j++) {
            if (strcmp((*lang_stats)[j].language, files[i].language) == 0) {
                found = 1;
                (*lang_stats)[j].file_count++;
                (*lang_stats)[j].total_lines += files[i].total_lines;
                (*lang_stats)[j].code_lines += files[i].code_lines;
                (*lang_stats)[j].comment_lines += files[i].comment_lines;
                (*lang_stats)[j].blank_lines += files[i].blank_lines;
                break;
            }
        }
        
        // 新语言
        if (!found) {
            if (*lang_count >= capacity) {
                capacity *= 2;
                *lang_stats = realloc(*lang_stats, sizeof(LanguageStats) * capacity);
            }
            
            LanguageStats *stat = &(*lang_stats)[*lang_count];
            strncpy(stat->language, files[i].language, sizeof(stat->language) - 1);
            stat->icon = files[i].icon;
            stat->file_count = 1;
            stat->total_lines = files[i].total_lines;
            stat->code_lines = files[i].code_lines;
            stat->comment_lines = files[i].comment_lines;
            stat->blank_lines = files[i].blank_lines;
            (*lang_count)++;
        }
    }
}

// 显示分隔线
static void print_separator(const char *color) {
    if (color) {
        color_println(color, "══════════════════════════════════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════════════════════════════════\n");
    }
}

// 显示表头
static void print_table_header(Options *opts, int show_lang) {
    if (opts->color) {
        color_print(COLOR_BRIGHT_CYAN, "%-4s", opts->show_icons ? " " : "");
        if (show_lang) {
            color_print(COLOR_BRIGHT_CYAN, "%-20s", "语言");
        } else {
            color_print(COLOR_BRIGHT_CYAN, "%-40s", "文件");
        }
        color_print(COLOR_BRIGHT_CYAN, "%8s %8s %8s %8s %8s %12s", 
                   "文件数", "总行数", "代码行", "注释", "空行", "占比");
        printf("\n");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("%-4s", opts->show_icons ? " " : "");
        if (show_lang) {
            printf("%-20s", "语言");
        } else {
            printf("%-40s", "文件");
        }
        printf("%8s %8s %8s %8s %8s %12s\n", 
               "文件数", "总行数", "代码行", "注释", "空行", "占比");
        print_separator(NULL);
    }
}

// 显示文件统计
static void show_file_stats(FileStats *files, int file_count, Options *opts) {
    if (file_count == 0) {
        print_info("未找到代码文件");
        return;
    }
    
    // 统计所有文件
    print_table_header(opts, 0);
    
    int total_files = 0;
    int total_lines = 0;
    int total_code = 0;
    int total_comment = 0;
    int total_blank = 0;
    
    for (int i = 0; i < file_count; i++) {
        // 获取文件名（不含路径）
        const char *filename = strrchr(files[i].filename, '/');
        if (filename == NULL) {
            filename = files[i].filename;
        } else {
            filename++; // 跳过 '/'
        }
        
        // 显示图标
        if (opts->show_icons) {
            printf("%-2s ", files[i].icon ? files[i].icon : "  ");
        }
        
        // 显示文件名（带颜色）
        if (opts->color) {
            color_print(COLOR_BRIGHT_GREEN, "%-38.38s", filename);
        } else {
            printf("%-38.38s", filename);
        }
        
        // 显示统计
        printf("%8d %8d %8d %8d %8d", 
               1, // 文件数
               files[i].total_lines,
               files[i].code_lines,
               files[i].comment_lines,
               files[i].blank_lines);
        
        // 显示百分比（如果启用）
        if (opts->show_percentage && files[i].total_lines > 0) {
            float code_pct = (float)files[i].code_lines / files[i].total_lines * 100;
            float comment_pct = (float)files[i].comment_lines / files[i].total_lines * 100;
            float blank_pct = (float)files[i].blank_lines / files[i].total_lines * 100;
            printf(" %3.0f%%/%2.0f%%/%2.0f%%", code_pct, comment_pct, blank_pct);
        }
        
        printf("\n");
        
        // 累加总计
        total_files++;
        total_lines += files[i].total_lines;
        total_code += files[i].code_lines;
        total_comment += files[i].comment_lines;
        total_blank += files[i].blank_lines;
    }
    
    // 显示总计
    printf("\n");
    if (opts->color) {
        color_print(COLOR_BRIGHT_YELLOW, "%-4s总计:%-34s", 
                   opts->show_icons ? " " : "", "");
    } else {
        printf("%-4s总计:%-34s", opts->show_icons ? " " : "", "");
    }
    printf("%8d %8d %8d %8d %8d", 
           total_files, total_lines, total_code, total_comment, total_blank);
    
    if (opts->show_percentage && total_lines > 0) {
        float code_pct = (float)total_code / total_lines * 100;
        float comment_pct = (float)total_comment / total_lines * 100;
        float blank_pct = (float)total_blank / total_lines * 100;
        printf(" %3.0f%%/%2.0f%%/%2.0f%%", code_pct, comment_pct, blank_pct);
    }
    printf("\n");
}

// 显示语言统计
static void show_language_stats(LanguageStats *lang_stats, int lang_count, Options *opts) {
    if (lang_count == 0) {
        print_info("未找到代码文件");
        return;
    }
    
    print_table_header(opts, 1);
    
    int total_files = 0;
    int total_lines = 0;
    int total_code = 0;
    int total_comment = 0;
    int total_blank = 0;
    
    for (int i = 0; i < lang_count; i++) {
        // 显示图标
        if (opts->show_icons) {
            printf("%-2s ", lang_stats[i].icon ? lang_stats[i].icon : "  ");
        }
        
        // 显示语言（带颜色）
        if (opts->color) {
            color_print(COLOR_BRIGHT_GREEN, "%-18.18s", lang_stats[i].language);
        } else {
            printf("%-18.18s", lang_stats[i].language);
        }
        
        // 显示统计
        printf("%8d %8d %8d %8d %8d", 
               lang_stats[i].file_count,
               lang_stats[i].total_lines,
               lang_stats[i].code_lines,
               lang_stats[i].comment_lines,
               lang_stats[i].blank_lines);
        
        // 显示百分比
        if (opts->show_percentage && lang_stats[i].total_lines > 0) {
            float code_pct = (float)lang_stats[i].code_lines / lang_stats[i].total_lines * 100;
            float comment_pct = (float)lang_stats[i].comment_lines / lang_stats[i].total_lines * 100;
            float blank_pct = (float)lang_stats[i].blank_lines / lang_stats[i].total_lines * 100;
            printf(" %3.0f%%/%2.0f%%/%2.0f%%", code_pct, comment_pct, blank_pct);
        }
        
        printf("\n");
        
        // 累加总计
        total_files += lang_stats[i].file_count;
        total_lines += lang_stats[i].total_lines;
        total_code += lang_stats[i].code_lines;
        total_comment += lang_stats[i].comment_lines;
        total_blank += lang_stats[i].blank_lines;
    }
    
    // 显示总计
    printf("\n");
    if (opts->color) {
        color_print(COLOR_BRIGHT_YELLOW, "%-4s总计:%-14s", 
                   opts->show_icons ? " " : "", "");
    } else {
        printf("%-4s总计:%-14s", opts->show_icons ? " " : "", "");
    }
    printf("%8d %8d %8d %8d %8d", 
           total_files, total_lines, total_code, total_comment, total_blank);
    
    if (opts->show_percentage && total_lines > 0) {
        float code_pct = (float)total_code / total_lines * 100;
        float comment_pct = (float)total_comment / total_lines * 100;
        float blank_pct = (float)total_blank / total_lines * 100;
        printf(" %3.0f%%/%2.0f%%/%2.0f%%", code_pct, comment_pct, blank_pct);
    }
    printf("\n");
}

// 显示简要汇总
static void show_summary(FileStats *files, int file_count, Options *opts) {
    if (file_count == 0) {
        print_info("未找到代码文件");
        return;
    }
    
    // 汇总统计
    int total_files = file_count;
    int total_lines = 0;
    int total_code = 0;
    int total_comment = 0;
    int total_blank = 0;
    
    for (int i = 0; i < file_count; i++) {
        total_lines += files[i].total_lines;
        total_code += files[i].code_lines;
        total_comment += files[i].comment_lines;
        total_blank += files[i].blank_lines;
    }
    
    // 显示标题
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "📊 代码统计汇总");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("代码统计汇总\n");
        print_separator(NULL);
    }
    
    // 显示统计
    printf("📁 文件总数:   %8d\n", total_files);
    printf("📝 总代码行数: %8d\n", total_lines);
    
    if (opts->color) {
        color_print(COLOR_BRIGHT_GREEN, "💻 代码行数:   %8d", total_code);
    } else {
        printf("代码行数:     %8d", total_code);
    }
    
    if (total_lines > 0) {
        float pct = (float)total_code / total_lines * 100;
        printf(" (%.1f%%)\n", pct);
    } else {
        printf("\n");
    }
    
    if (opts->color) {
        color_print(COLOR_BRIGHT_YELLOW, "💬 注释行数:   %8d", total_comment);
    } else {
        printf("注释行数:     %8d", total_comment);
    }
    
    if (total_lines > 0) {
        float pct = (float)total_comment / total_lines * 100;
        printf(" (%.1f%%)\n", pct);
    } else {
        printf("\n");
    }
    
    printf("⬜ 空行行数:   %8d", total_blank);
    if (total_lines > 0) {
        float pct = (float)total_blank / total_lines * 100;
        printf(" (%.1f%%)\n", pct);
    } else {
        printf("\n");
    }
    
    // 代码注释比
    if (total_code > 0) {
        float ratio = (float)total_comment / total_code;
        printf("📈 注释密度:   %8.2f (注释/代码)\n", ratio);
    }
    
    printf("\n");
}

// tkcode主函数
int tkcode_main(int argc, char **argv) {
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
    
    // 收集所有代码文件
    FileStats *all_files = NULL;
    int total_file_count = 0;
    int capacity = 16;
    all_files = malloc(sizeof(FileStats) * capacity);
    
    for (int i = 0; i < opts.path_count; i++) {
        FileStats *files;
        int file_count;
        
        if (collect_code_files(opts.paths[i], &opts, &files, &file_count)) {
            // 扩展数组
            while (total_file_count + file_count >= capacity) {
                capacity *= 2;
                all_files = realloc(all_files, sizeof(FileStats) * capacity);
            }
            
            // 复制文件
            memcpy(all_files + total_file_count, files, sizeof(FileStats) * file_count);
            total_file_count += file_count;
            free(files);
        }
    }
    
    if (total_file_count == 0) {
        print_error("未找到代码文件");
        if (all_files) free(all_files);
        if (opts.paths) free(opts.paths);
        return 1;
    }
    
    // 统计每个文件
    for (int i = 0; i < total_file_count; i++) {
        count_file_lines(all_files[i].filename, &all_files[i]);
    }
    
    // 显示标题
    if (opts.color) {
        printf("\n");
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_MAGENTA, "                              📊 代码统计分析");
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════════════════════");
        printf("\n");
    } else {
        printf("\n");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
        printf("                              代码统计分析\n");
        printf("══════════════════════════════════════════════════════════════════════════════\n\n");
    }
    
    // 根据选项显示统计
    if (opts.summary_only) {
        show_summary(all_files, total_file_count, &opts);
    } else if (opts.by_language) {
        LanguageStats *lang_stats;
        int lang_count;
        aggregate_by_language(all_files, total_file_count, &lang_stats, &lang_count);
        show_language_stats(lang_stats, lang_count, &opts);
        free(lang_stats);
    } else {
        show_file_stats(all_files, total_file_count, &opts);
    }
    
    // 清理
    free(all_files);
    if (opts.paths) free(opts.paths);
    
    return 0;
}