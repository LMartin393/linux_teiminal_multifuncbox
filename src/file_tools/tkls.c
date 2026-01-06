// src/file_tools/tkls.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 文件信息结构
typedef struct {
    char name[256];
    struct stat info;
    int is_hidden;
    char type_indicator;
} FileEntry;

// 选项结构
typedef struct {
    int show_all;        // -a 显示所有文件
    int long_format;     // -l 长格式
    int human_size;      // -h 人类可读大小
    int recursive;       // -R 递归
    int sort_by_time;    // -t 按时间排序
    int reverse_sort;    // -r 反向排序
    int classify;        // -F 添加类型标识
    int show_inode;      // -i 显示inode
    int one_per_line;    // -1 每行一个
    int color;           // 颜色显示
    int show_icons;      // 显示图标
    int show_git;        // 显示git状态（预留）
} Options;

// 初始化选项
static void init_options(Options *opts) {
    opts->show_all = 0;
    opts->long_format = 0;
    opts->human_size = 0;
    opts->recursive = 0;
    opts->sort_by_time = 0;
    opts->reverse_sort = 0;
    opts->classify = 0;
    opts->show_inode = 0;
    opts->one_per_line = 0;
    opts->color = is_color_supported();  // 自动检测颜色支持
    opts->show_icons = 1;  // 默认显示图标
    opts->show_git = 0;    // 简化版不显示git状态
}

// 显示帮助
static void show_help(void) {
    color_println(COLOR_BRIGHT_CYAN, "tkls - 增强版ls工具（带图标）");
    printf("\n");
    printf("用法: tkls [选项]... [目录]...\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "选项:");
    printf("  -a, --all          显示所有文件，包括隐藏文件\n");
    printf("  -l                 使用长列表格式\n");
    printf("  -h, --human-readable  以易读格式显示文件大小\n");
    printf("  -R, --recursive    递归显示子目录\n");
    printf("  -t                 按修改时间排序\n");
    printf("  -r, --reverse      反向排序\n");
    printf("  -F, --classify     添加文件类型标识符 (*/@/=等)\n");
    printf("  -i                 显示inode号\n");
    printf("  -1                 每行只显示一个文件\n");
    printf("      --no-color     禁用彩色输出\n");
    printf("      --no-icons     禁用图标显示\n");
    printf("      --help         显示此帮助信息\n");
    printf("      --version      显示版本信息\n");
    printf("\n");
    color_println(COLOR_BRIGHT_GREEN, "图标说明:");
    printf("  📁 目录    📄 普通文件    ⚡ 可执行文件\n");
    printf("  🔗 链接    💿 设备文件    🎵 音乐文件\n");
    printf("  🖼️  图片    📖 文档文件    🗜️  压缩文件\n");
    printf("\n");
    color_println(COLOR_BRIGHT_GREEN, "示例:");
    printf("  tkls               列出当前目录（带图标）\n");
    printf("  tkls -l           长格式列表\n");
    printf("  tkls -la          显示所有文件（包括隐藏文件）\n");
    printf("  tkls /home        列出指定目录\n");
}

// 显示版本
static void show_version(void) {
    color_println(COLOR_BRIGHT_MAGENTA, "tkls - TermKit 增强版ls工具");
    printf("版本: 1.0.0\n");
    printf("功能: 彩色输出、文件图标、智能布局\n");
}

// 解析参数
static int parse_options(int argc, char **argv, Options *opts, char ***paths, int *path_count) {
    *paths = NULL;
    *path_count = 0;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
                opts->show_all = 1;
            } else if (strcmp(argv[i], "-l") == 0) {
                opts->long_format = 1;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--human-readable") == 0) {
                opts->human_size = 1;
            } else if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--recursive") == 0) {
                opts->recursive = 1;
            } else if (strcmp(argv[i], "-t") == 0) {
                opts->sort_by_time = 1;
            } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--reverse") == 0) {
                opts->reverse_sort = 1;
            } else if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--classify") == 0) {
                opts->classify = 1;
            } else if (strcmp(argv[i], "-i") == 0) {
                opts->show_inode = 1;
            } else if (strcmp(argv[i], "-1") == 0) {
                opts->one_per_line = 1;
            } else if (strcmp(argv[i], "--no-color") == 0) {
                opts->color = 0;
            } else if (strcmp(argv[i], "--no-icons") == 0) {
                opts->show_icons = 0;
            } else if (strcmp(argv[i], "--help") == 0) {
                show_help();
                return 0; // 特殊返回，表示不需要继续执行
            } else if (strcmp(argv[i], "--version") == 0) {
                show_version();
                return 0; // 特殊返回，表示不需要继续执行
            } else {
                print_error("无效选项: %s", argv[i]);
                printf("使用 'tkls --help' 查看帮助\n");
                return -1; // 错误返回
            }
        } else {
            // 路径参数
            (*path_count)++;
            *paths = realloc(*paths, sizeof(char*) * (*path_count));
            if (*paths == NULL) {
                print_error("内存分配失败");
                return -1;
            }
            (*paths)[*path_count - 1] = argv[i];
        }
    }
    
    // 如果没有指定路径，使用当前目录
    if (*path_count == 0) {
        *path_count = 1;
        *paths = malloc(sizeof(char*));
        if (*paths == NULL) {
            print_error("内存分配失败");
            return -1;
        }
        (*paths)[0] = ".";
    }
    
    return 1; // 正常返回
}

// 获取文件类型标识符
static char get_type_indicator(mode_t mode) {
    if (S_ISDIR(mode)) return '/';
    if (S_ISLNK(mode)) return '@';
    if (S_ISFIFO(mode)) return '|';
    if (S_ISSOCK(mode)) return '=';
    if (mode & S_IXUSR) return '*';
    return ' ';
}

// 根据扩展名获取图标
static const char* get_icon_by_extension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) return "📄 ";  // 默认文件图标
    
    ext++;  // 跳过点号
    
    // 图片文件
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
        strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "svg") == 0) {
        return "🖼️  ";
    }
    
    // 文档文件
    if (strcasecmp(ext, "pdf") == 0 || strcasecmp(ext, "doc") == 0 ||
        strcasecmp(ext, "docx") == 0 || strcasecmp(ext, "txt") == 0 ||
        strcasecmp(ext, "md") == 0 || strcasecmp(ext, "rtf") == 0) {
        return "📖 ";
    }
    
    // 代码文件
    if (strcasecmp(ext, "c") == 0 || strcasecmp(ext, "cpp") == 0 ||
        strcasecmp(ext, "h") == 0 || strcasecmp(ext, "hpp") == 0 ||
        strcasecmp(ext, "py") == 0 || strcasecmp(ext, "java") == 0 ||
        strcasecmp(ext, "js") == 0 || strcasecmp(ext, "html") == 0 ||
        strcasecmp(ext, "css") == 0) {
        return "📝 ";
    }
    
    // 压缩文件
    if (strcasecmp(ext, "zip") == 0 || strcasecmp(ext, "tar") == 0 ||
        strcasecmp(ext, "gz") == 0 || strcasecmp(ext, "bz2") == 0 ||
        strcasecmp(ext, "7z") == 0 || strcasecmp(ext, "rar") == 0) {
        return "🗜️  ";
    }
    
    // 媒体文件
    if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "wav") == 0 ||
        strcasecmp(ext, "flac") == 0 || strcasecmp(ext, "m4a") == 0) {
        return "🎵 ";
    }
    
    if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0 ||
        strcasecmp(ext, "mkv") == 0 || strcasecmp(ext, "mov") == 0) {
        return "🎬 ";
    }
    
    // 配置文件
    if (strcasecmp(ext, "conf") == 0 || strcasecmp(ext, "config") == 0 ||
        strcasecmp(ext, "ini") == 0 || strcasecmp(ext, "json") == 0 ||
        strcasecmp(ext, "xml") == 0 || strcasecmp(ext, "yaml") == 0 ||
        strcasecmp(ext, "yml") == 0) {
        return "⚙️  ";
    }
    
    return "📄 ";  // 默认文件图标
}

// 获取文件图标
static const char* get_file_icon(mode_t mode, const char *filename) {
    if (S_ISDIR(mode)) return "📁 ";
    if (S_ISLNK(mode)) return "🔗 ";
    if (S_ISCHR(mode) || S_ISBLK(mode)) return "💿 ";
    if (S_ISSOCK(mode)) return "🔌 ";
    if (S_ISFIFO(mode)) return "📫 ";
    if (mode & S_IXUSR) return "⚡ ";
    
    // 普通文件根据扩展名显示不同图标
    return get_icon_by_extension(filename);
}

// 获取文件颜色
static const char* get_file_color(mode_t mode) {
    if (!is_color_supported()) return NULL;
    
    if (S_ISDIR(mode)) return COLOR_BRIGHT_BLUE;
    if (S_ISLNK(mode)) return COLOR_BRIGHT_CYAN;
    if (mode & S_IXUSR) return COLOR_BRIGHT_GREEN;
    if (S_ISCHR(mode) || S_ISBLK(mode)) return COLOR_BRIGHT_YELLOW;
    if (S_ISSOCK(mode)) return COLOR_MAGENTA;
    if (S_ISFIFO(mode)) return COLOR_YELLOW;
    return COLOR_WHITE;
}

// 收集目录中的文件
static int collect_files(const char *path, Options *opts, FileEntry **files) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        print_error("无法打开目录 '%s': %s", path, strerror(errno));
        return 0;
    }
    
    int count = 0;
    int capacity = 16;
    *files = malloc(sizeof(FileEntry) * capacity);
    if (*files == NULL) {
        closedir(dir);
        print_error("内存分配失败");
        return 0;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..（除非指定 -a）
        if (!opts->show_all && 
            (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }
        
        // 跳过隐藏文件（除非指定 -a）
        if (!opts->show_all && entry->d_name[0] == '.') {
            continue;
        }
        
        // 扩展数组
        if (count >= capacity) {
            capacity *= 2;
            *files = realloc(*files, sizeof(FileEntry) * capacity);
            if (*files == NULL) {
                closedir(dir);
                print_error("内存分配失败");
                return 0;
            }
        }
        
        FileEntry *fe = &(*files)[count];
        strncpy(fe->name, entry->d_name, sizeof(fe->name) - 1);
        fe->name[sizeof(fe->name) - 1] = '\0';
        fe->is_hidden = (entry->d_name[0] == '.');
        
        // 获取文件信息
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (lstat(full_path, &fe->info) == -1) {
            fe->type_indicator = '?';
        } else {
            fe->type_indicator = get_type_indicator(fe->info.st_mode);
        }
        
        count++;
    }
    
    closedir(dir);
    return count;
}

// 比较函数：按名称
static int compare_name(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    return strcasecmp(fa->name, fb->name);
}

// 比较函数：按时间
static int compare_time(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    if (fa->info.st_mtime < fb->info.st_mtime) return 1;
    if (fa->info.st_mtime > fb->info.st_mtime) return -1;
    return 0;
}

// 排序文件
static void sort_files(FileEntry *files, int count, Options *opts) {
    if (opts->sort_by_time) {
        qsort(files, count, sizeof(FileEntry), compare_time);
    } else {
        qsort(files, count, sizeof(FileEntry), compare_name);
    }
    
    if (opts->reverse_sort) {
        for (int i = 0; i < count / 2; i++) {
            FileEntry temp = files[i];
            files[i] = files[count - i - 1];
            files[count - i - 1] = temp;
        }
    }
}

// 获取权限字符串
static void get_permission_string(mode_t mode, char *perm) {
    perm[0] = S_ISDIR(mode) ? 'd' : 
              S_ISLNK(mode) ? 'l' : 
              S_ISCHR(mode) ? 'c' : 
              S_ISBLK(mode) ? 'b' : 
              S_ISFIFO(mode) ? 'p' : 
              S_ISSOCK(mode) ? 's' : '-';
    
    perm[1] = (mode & S_IRUSR) ? 'r' : '-';
    perm[2] = (mode & S_IWUSR) ? 'w' : '-';
    perm[3] = (mode & S_IXUSR) ? 'x' : '-';
    perm[4] = (mode & S_IRGRP) ? 'r' : '-';
    perm[5] = (mode & S_IWGRP) ? 'w' : '-';
    perm[6] = (mode & S_IXGRP) ? 'x' : '-';
    perm[7] = (mode & S_IROTH) ? 'r' : '-';
    perm[8] = (mode & S_IWOTH) ? 'w' : '-';
    perm[9] = (mode & S_IXOTH) ? 'x' : '-';
    perm[10] = '\0';
}

// 打印长格式（带图标）
static void print_long_format(FileEntry *files, int count, Options *opts) {
    // 计算总块数
    long total_blocks = 0;
    for (int i = 0; i < count; i++) {
        total_blocks += files[i].info.st_blocks;
    }
    
    if (count > 0) {
        color_print(COLOR_BRIGHT_BLUE, "总计 %ld", total_blocks / 2);
        printf("\n");
    }
    
    for (int i = 0; i < count; i++) {
        FileEntry *fe = &files[i];
        
        // inode号
        if (opts->show_inode) {
            printf("%8lu ", (unsigned long)fe->info.st_ino);
        }
        
        // 权限
        char perm[11];
        get_permission_string(fe->info.st_mode, perm);
        printf("%s ", perm);
        
        // 链接数
        printf("%3ld ", (long)fe->info.st_nlink);
        
        // 用户和组
        struct passwd *pw = getpwuid(fe->info.st_uid);
        struct group *gr = getgrgid(fe->info.st_gid);
        printf("%-8s %-8s ", pw ? pw->pw_name : "?", gr ? gr->gr_name : "?");
        
        // 大小
        if (opts->human_size) {
            char *size_str = format_size(fe->info.st_size);
            printf("%8s ", size_str);
        } else {
            printf("%8ld ", (long)fe->info.st_size);
        }
        
        // 时间
        char *time_str = format_time(fe->info.st_mtime);
        printf("%s ", time_str);
        
        // 图标（如果启用）
        if (opts->show_icons) {
            printf("%s", get_file_icon(fe->info.st_mode, fe->name));
        }
        
        // 文件名（带颜色）
        const char *color = opts->color ? get_file_color(fe->info.st_mode) : NULL;
        if (color) {
            color_print(color, "%s", fe->name);
        } else {
            printf("%s", fe->name);
        }
        
        // 类型标识符
        if (opts->classify && fe->type_indicator != ' ') {
            printf("%c", fe->type_indicator);
        }
        
        printf("\n");
    }
}

// 打印网格格式（带图标）
static void print_grid_format(FileEntry *files, int count, Options *opts) {
    // 获取终端宽度
    struct winsize w;
    int term_width = 80; // 默认值
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        term_width = w.ws_col;
    }
    
    // 计算最大显示长度（包括图标）
    int max_len = 0;
    for (int i = 0; i < count; i++) {
        int len = strlen(files[i].name);
        if (opts->show_icons) len += 3; // 图标占3个字符宽度
        if (opts->classify && files[i].type_indicator != ' ') len += 1;
        if (len > max_len) max_len = len;
    }
    
    max_len += 2; // 间距
    
    // 计算列数
    int cols = term_width / max_len;
    if (cols == 0) cols = 1;
    int rows = (count + cols - 1) / cols;
    
    // 打印
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int idx = row + col * rows;
            if (idx >= count) continue;
            
            FileEntry *fe = &files[idx];
            
            // 图标（如果启用）
            if (opts->show_icons) {
                printf("%s", get_file_icon(fe->info.st_mode, fe->name));
            }
            
            // 颜色
            const char *color = opts->color ? get_file_color(fe->info.st_mode) : NULL;
            if (color) {
                color_print(color, "%s", fe->name);
            } else {
                printf("%s", fe->name);
            }
            
            // 类型标识符
            if (opts->classify && fe->type_indicator != ' ') {
                printf("%c", fe->type_indicator);
            }
            
            // 填充空格
            int name_len = strlen(fe->name);
            if (opts->show_icons) name_len += 3; // 图标宽度
            if (opts->classify && fe->type_indicator != ' ') name_len += 1;
            
            for (int j = name_len; j < max_len; j++) {
                printf(" ");
            }
        }
        printf("\n");
    }
}

// 列出单个目录
static int list_directory(const char *path, Options *opts) {
    FileEntry *files;
    int count = collect_files(path, opts, &files);
    if (count == 0) {
        // 可能是空目录或出错
        free(files);
        return 0;
    }
    
    // 排序
    sort_files(files, count, opts);
    
    // 打印
    if (opts->long_format || opts->one_per_line) {
        print_long_format(files, count, opts);
    } else {
        print_grid_format(files, count, opts);
    }
    
    free(files);
    return 1;
}

// 递归列出目录（带图标）
static void list_recursive(const char *path, Options *opts, int depth) {
    // 首先列出当前目录
    FileEntry *files;
    int count = collect_files(path, opts, &files);
    if (count == 0) {
        free(files);
        return;
    }
    
    sort_files(files, count, opts);
    
    // 缩进和目录名
    for (int i = 0; i < depth; i++) printf("  ");
    color_println(COLOR_BRIGHT_BLUE, "%s:", path);
    
    if (opts->long_format || opts->one_per_line) {
        print_long_format(files, count, opts);
    } else {
        print_grid_format(files, count, opts);
    }
    
    printf("\n");
    
    // 递归处理子目录
    for (int i = 0; i < count; i++) {
        if (S_ISDIR(files[i].info.st_mode) && 
            strcmp(files[i].name, ".") != 0 && 
            strcmp(files[i].name, "..") != 0) {
            
            char sub_path[1024];
            snprintf(sub_path, sizeof(sub_path), "%s/%s", path, files[i].name);
            list_recursive(sub_path, opts, depth + 1);
        }
    }
    
    free(files);
}

// tkls主函数
int tkls_main(int argc, char **argv) {
    Options opts;
    init_options(&opts);
    
    char **paths = NULL;
    int path_count = 0;
    
    int parse_result = parse_options(argc, argv, &opts, &paths, &path_count);
    if (parse_result <= 0) {
        if (paths) free(paths);
        return parse_result == -1 ? 1 : 0;
    }
    
    int exit_code = 0;
    
    for (int i = 0; i < path_count; i++) {
        const char *path = paths[i];
        
        // 检查路径是否存在
        if (!file_exists(path)) {
            print_error("无法访问 '%s': 没有那个文件或目录", path);
            exit_code = 1;
            continue;
        }
        
        // 如果是目录
        if (is_directory(path)) {
            if (path_count > 1) {
                if (i > 0) printf("\n");
                color_println(COLOR_BRIGHT_BLUE, "%s:", path);
            }
            
            if (opts.recursive) {
                list_recursive(path, &opts, 0);
            } else {
                if (!list_directory(path, &opts)) {
                    // 可能是空目录或出错
                    if (errno != 0) {
                        exit_code = 1;
                    }
                }
            }
        } else {
            // 单个文件
            struct stat st;
            if (lstat(path, &st) == -1) {
                print_error("无法访问 '%s': %s", path, strerror(errno));
                exit_code = 1;
                continue;
            }
            
            // 创建一个FileEntry结构
            FileEntry file;
            const char *basename = strrchr(path, '/');
            if (basename) {
                strncpy(file.name, basename + 1, sizeof(file.name) - 1);
            } else {
                strncpy(file.name, path, sizeof(file.name) - 1);
            }
            file.name[sizeof(file.name) - 1] = '\0';
            file.info = st;
            file.is_hidden = (path[0] == '.');
            file.type_indicator = get_type_indicator(st.st_mode);
            
            FileEntry *files = &file;
            
            if (opts.long_format || opts.one_per_line) {
                print_long_format(files, 1, &opts);
            } else {
                // 图标
                if (opts.show_icons) {
                    printf("%s", get_file_icon(st.st_mode, file.name));
                }
                
                // 颜色
                const char *color = opts.color ? get_file_color(st.st_mode) : NULL;
                if (color) {
                    color_println(color, "%s", file.name);
                } else {
                    printf("%s\n", file.name);
                }
            }
        }
    }
    
    if (paths) free(paths);
    return exit_code;
}