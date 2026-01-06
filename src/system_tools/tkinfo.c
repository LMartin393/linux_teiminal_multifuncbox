// src/system_tools/tkinfo.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <pwd.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 选项结构
typedef struct {
    int brief;           // 简要信息
    int detailed;        // 详细信息
    int cpu_info;        // CPU信息
    int memory_info;     // 内存信息
    int disk_info;       // 磁盘信息
    int network_info;    // 网络信息
    int user_info;       // 用户信息
    int process_info;    // 进程信息
    int uptime_info;     // 运行时间
    int os_info;         // 系统信息
    int color;           // 彩色输出
    int help;            // 帮助信息
    int version;         // 版本信息
} Options;

// 初始化选项
static void init_options(Options *opts) {
    opts->brief = 0;
    opts->detailed = 0;
    opts->cpu_info = 0;
    opts->memory_info = 0;
    opts->disk_info = 0;
    opts->network_info = 0;
    opts->user_info = 0;
    opts->process_info = 0;
    opts->uptime_info = 0;
    opts->os_info = 0;
    opts->color = is_color_supported();
    opts->help = 0;
    opts->version = 0;
}

// 显示帮助
static void show_help(void) {
    color_println(COLOR_BRIGHT_CYAN, "tkinfo - 系统信息显示工具");
    printf("\n");
    printf("用法: tkinfo [选项]\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "信息类别:");
    printf("  -a, --all            显示所有信息\n");
    printf("  -b, --brief          显示简要信息（默认）\n");
    printf("  -d, --detailed       显示详细信息\n");
    printf("  -c, --cpu            显示CPU信息\n");
    printf("  -m, --memory         显示内存信息\n");
    printf("  -s, --disk           显示磁盘信息\n");
    printf("  -n, --network        显示网络信息\n");
    printf("  -u, --user           显示用户信息\n");
    printf("  -p, --process        显示进程信息\n");
    printf("  -t, --uptime         显示运行时间\n");
    printf("  -o, --os             显示系统信息\n");
    printf("\n");
    color_println(COLOR_BRIGHT_YELLOW, "显示选项:");
    printf("      --color          彩色输出（默认）\n");
    printf("      --no-color       黑白输出\n");
    printf("      --help           显示此帮助\n");
    printf("      --version        显示版本\n");
    printf("\n");
    color_println(COLOR_BRIGHT_GREEN, "示例:");
    printf("  tkinfo               显示简要系统信息\n");
    printf("  tkinfo -a            显示所有系统信息\n");
    printf("  tkinfo -c -m         显示CPU和内存信息\n");
    printf("  tkinfo --no-color    不使用彩色输出\n");
}

// 显示版本
static void show_version(void) {
    color_println(COLOR_BRIGHT_MAGENTA, "tkinfo - TermKit 系统信息工具");
    printf("版本: 1.0.0\n");
    printf("功能: 美观的系统信息显示，支持彩色输出\n");
}

// 解析选项
static int parse_options(int argc, char **argv, Options *opts) {
    // 默认显示简要信息
    opts->brief = 1;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
                opts->brief = 0;
                opts->detailed = 1;
                opts->cpu_info = 1;
                opts->memory_info = 1;
                opts->disk_info = 1;
                opts->network_info = 1;
                opts->user_info = 1;
                opts->process_info = 1;
                opts->uptime_info = 1;
                opts->os_info = 1;
            } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--brief") == 0) {
                opts->brief = 1;
                opts->detailed = 0;
            } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--detailed") == 0) {
                opts->brief = 0;
                opts->detailed = 1;
            } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cpu") == 0) {
                opts->brief = 0;
                opts->cpu_info = 1;
            } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--memory") == 0) {
                opts->brief = 0;
                opts->memory_info = 1;
            } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--disk") == 0) {
                opts->brief = 0;
                opts->disk_info = 1;
            } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--network") == 0) {
                opts->brief = 0;
                opts->network_info = 1;
            } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--user") == 0) {
                opts->brief = 0;
                opts->user_info = 1;
            } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--process") == 0) {
                opts->brief = 0;
                opts->process_info = 1;
            } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--uptime") == 0) {
                opts->brief = 0;
                opts->uptime_info = 1;
            } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--os") == 0) {
                opts->brief = 0;
                opts->os_info = 1;
            } else if (strcmp(argv[i], "--color") == 0) {
                opts->color = 1;
            } else if (strcmp(argv[i], "--no-color") == 0) {
                opts->color = 0;
            } else if (strcmp(argv[i], "--help") == 0) {
                opts->help = 1;
                return 1;
            } else if (strcmp(argv[i], "--version") == 0) {
                opts->version = 1;
                return 1;
            } else {
                print_error("无效选项: %s", argv[i]);
                printf("使用 'tkinfo --help' 查看帮助\n");
                return -1;
            }
        } else {
            print_error("无效参数: %s", argv[i]);
            printf("使用 'tkinfo --help' 查看帮助\n");
            return -1;
        }
    }
    
    return 1;
}

// 显示分隔线
static void print_separator(const char *color) {
    if (color) {
        color_println(color, "══════════════════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
    }
}

// 显示信息项
static void print_info_item(const char *label, const char *value, const char *color) {
    if (color) {
        color_print(color, "  %-18s", label);
        printf("%s\n", value);
    } else {
        printf("  %-18s%s\n", label, value);
    }
}

// 获取CPU核心数
static int get_cpu_core_count(void) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) return 1;
    
    int cores = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "processor") != NULL) {
            cores++;
        }
    }
    
    fclose(fp);
    return cores > 0 ? cores : 1;
}

// 获取CPU型号
static char* get_cpu_model(void) {
    static char model[256] = "Unknown";
    FILE *fp = fopen("/proc/cpuinfo", "r");
    
    if (fp != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "model name") != NULL) {
                char *colon = strchr(line, ':');
                if (colon != NULL) {
                    strncpy(model, colon + 2, sizeof(model) - 1);
                    model[sizeof(model) - 1] = '\0';
                    
                    // 去除换行符
                    char *newline = strchr(model, '\n');
                    if (newline != NULL) *newline = '\0';
                    break;
                }
            }
        }
        fclose(fp);
    }
    
    return model;
}

// 显示CPU信息
static void show_cpu_info(Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🖥️  CPU信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("CPU信息:\n");
        print_separator(NULL);
    }
    
    int cores = get_cpu_core_count();
    char *model = get_cpu_model();
    
    char cores_str[32];
    snprintf(cores_str, sizeof(cores_str), "%d 核心", cores);
    
    print_info_item("CPU型号:", model, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("核心数量:", cores_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    // 获取CPU频率（简化）
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "cpu MHz") != NULL) {
                char *colon = strchr(line, ':');
                if (colon != NULL) {
                    float mhz = atof(colon + 2);
                    char freq[32];
                    snprintf(freq, sizeof(freq), "%.2f GHz", mhz / 1000.0);
                    print_info_item("CPU频率:", freq, opts->color ? COLOR_BRIGHT_GREEN : NULL);
                    break;
                }
            }
        }
        fclose(fp);
    }
    
    printf("\n");
}

// 获取内存信息
static void get_memory_info(unsigned long *total, unsigned long *free, 
                           unsigned long *available, unsigned long *used) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        *total = *free = *available = *used = 0;
        return;
    }
    
    char line[256];
    *total = *free = *available = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "MemTotal:") != NULL) {
            sscanf(line, "MemTotal: %lu kB", total);
        } else if (strstr(line, "MemFree:") != NULL) {
            sscanf(line, "MemFree: %lu kB", free);
        } else if (strstr(line, "MemAvailable:") != NULL) {
            sscanf(line, "MemAvailable: %lu kB", available);
        }
    }
    
    fclose(fp);
    
    if (*total > 0) {
        *used = *total - (*available > 0 ? *available : *free);
    }
}

// 显示内存信息
static void show_memory_info(Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "💾 内存信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("内存信息:\n");
        print_separator(NULL);
    }
    
    unsigned long total_kb, free_kb, available_kb, used_kb;
    get_memory_info(&total_kb, &free_kb, &available_kb, &used_kb);
    
    char *total_str = format_size(total_kb * 1024);
    char *used_str = format_size(used_kb * 1024);
    char *free_str = format_size(free_kb * 1024);
    char *available_str = format_size(available_kb * 1024);
    
    float usage_percent = total_kb > 0 ? (float)used_kb / total_kb * 100.0f : 0;
    
    print_info_item("总内存:", total_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("已使用:", used_str, opts->color ? COLOR_BRIGHT_RED : NULL);
    print_info_item("可用内存:", available_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("空闲内存:", free_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    char usage_str[32];
    snprintf(usage_str, sizeof(usage_str), "%.1f%%", usage_percent);
    print_info_item("使用率:", usage_str, opts->color ? 
                   (usage_percent > 80 ? COLOR_BRIGHT_RED : 
                    usage_percent > 60 ? COLOR_BRIGHT_YELLOW : 
                    COLOR_BRIGHT_GREEN) : NULL);
    
    printf("\n");
}

// 获取磁盘信息
static void get_disk_info(unsigned long *total, unsigned long *free, 
                         unsigned long *used, float *usage_percent) {
    struct statvfs buf;
    
    if (statvfs("/", &buf) == 0) {
        *total = buf.f_blocks * buf.f_frsize;
        *free = buf.f_bfree * buf.f_frsize;
        *used = *total - *free;
        
        if (*total > 0) {
            *usage_percent = (float)(*total - *free) / *total * 100.0f;
        } else {
            *usage_percent = 0;
        }
    } else {
        *total = *free = *used = 0;
        *usage_percent = 0;
    }
}

// 显示磁盘信息
static void show_disk_info(Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "💽 磁盘信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("磁盘信息:\n");
        print_separator(NULL);
    }
    
    unsigned long total_bytes, free_bytes, used_bytes;
    float usage_percent;
    
    get_disk_info(&total_bytes, &free_bytes, &used_bytes, &usage_percent);
    
    char *total_str = format_size(total_bytes);
    char *used_str = format_size(used_bytes);
    char *free_str = format_size(free_bytes);
    
    print_info_item("磁盘总空间:", total_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("已用空间:", used_str, opts->color ? COLOR_BRIGHT_RED : NULL);
    print_info_item("可用空间:", free_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    char usage_str[32];
    snprintf(usage_str, sizeof(usage_str), "%.1f%%", usage_percent);
    print_info_item("使用率:", usage_str, opts->color ? 
                   (usage_percent > 90 ? COLOR_BRIGHT_RED : 
                    usage_percent > 80 ? COLOR_BRIGHT_YELLOW : 
                    COLOR_BRIGHT_GREEN) : NULL);
    
    // 显示inode信息
    struct statvfs buf;
    if (statvfs("/", &buf) == 0) {
        unsigned long total_inodes = buf.f_files;
        unsigned long free_inodes = buf.f_ffree;
        unsigned long used_inodes = total_inodes - free_inodes;
        float inode_usage = total_inodes > 0 ? 
                           (float)used_inodes / total_inodes * 100.0f : 0;
        
        char inode_usage_str[32];
        snprintf(inode_usage_str, sizeof(inode_usage_str), "%.1f%%", inode_usage);
        print_info_item("Inode使用率:", inode_usage_str, opts->color ? 
                       (inode_usage > 90 ? COLOR_BRIGHT_RED : 
                        inode_usage > 80 ? COLOR_BRIGHT_YELLOW : 
                        COLOR_BRIGHT_GREEN) : NULL);
    }
    
    printf("\n");
}

// 显示网络信息（简化）
static void show_network_info(Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🌐 网络信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("网络信息:\n");
        print_separator(NULL);
    }
    
    // 获取主机名
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        print_info_item("主机名:", hostname, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    }
    
    // 获取IP地址（简化，通过hostname获取）
    FILE *fp = popen("hostname -I 2>/dev/null || echo '未知'", "r");
    if (fp != NULL) {
        char ip[256];
        if (fgets(ip, sizeof(ip), fp) != NULL) {
            // 去除换行符
            char *newline = strchr(ip, '\n');
            if (newline != NULL) *newline = '\0';
            print_info_item("IP地址:", ip, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        }
        pclose(fp);
    }
    
    printf("\n");
}

// 显示用户信息
static void show_user_info(Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "👤 用户信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("用户信息:\n");
        print_separator(NULL);
    }
    
    // 当前用户
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    
    if (pw != NULL) {
        print_info_item("用户名:", pw->pw_name, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        print_info_item("用户ID:", pw->pw_gecos ? pw->pw_gecos : "N/A", 
                       opts->color ? COLOR_BRIGHT_GREEN : NULL);
        print_info_item("家目录:", pw->pw_dir, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    }
    
    // 登录用户数（简化）
    FILE *fp = popen("who | wc -l", "r");
    if (fp != NULL) {
        char users[32];
        if (fgets(users, sizeof(users), fp) != NULL) {
            char *newline = strchr(users, '\n');
            if (newline != NULL) *newline = '\0';
            print_info_item("登录用户:", users, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        }
        pclose(fp);
    }
    
    printf("\n");
}

// 显示进程信息（简化）
static void show_process_info(Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "⚡ 进程信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("进程信息:\n");
        print_separator(NULL);
    }
    
    // 统计进程数量
    int process_count = 0;
    DIR *dir = opendir("/proc");
    
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (isdigit(entry->d_name[0])) {
                process_count++;
            }
        }
        closedir(dir);
    }
    
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "%d", process_count);
    print_info_item("进程总数:", count_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    // 当前进程ID
    pid_t pid = getpid();
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    print_info_item("当前进程ID:", pid_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    printf("\n");
}

// 显示运行时间
static void show_uptime_info(Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "⏱️  运行时间:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("运行时间:\n");
        print_separator(NULL);
    }
    
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        long uptime_seconds = info.uptime;
        
        long days = uptime_seconds / 86400;
        long hours = (uptime_seconds % 86400) / 3600;
        long minutes = (uptime_seconds % 3600) / 60;
        long seconds = uptime_seconds % 60;
        
        char uptime_str[128];
        if (days > 0) {
            snprintf(uptime_str, sizeof(uptime_str), 
                    "%ld天 %02ld:%02ld:%02ld", days, hours, minutes, seconds);
        } else if (hours > 0) {
            snprintf(uptime_str, sizeof(uptime_str), 
                    "%02ld:%02ld:%02ld", hours, minutes, seconds);
        } else {
            snprintf(uptime_str, sizeof(uptime_str), 
                    "%02ld:%02ld", minutes, seconds);
        }
        
        print_info_item("运行时间:", uptime_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        
        // 系统负载
        char load_str[64];
        snprintf(load_str, sizeof(load_str), 
                "%.2f, %.2f, %.2f", 
                info.loads[0] / 65536.0,
                info.loads[1] / 65536.0,
                info.loads[2] / 65536.0);
        print_info_item("系统负载:", load_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    }
    
    printf("\n");
}

// 显示系统信息
static void show_os_info(Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🐧 系统信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("系统信息:\n");
        print_separator(NULL);
    }
    
    struct utsname info;
    if (uname(&info) == 0) {
        print_info_item("系统名称:", info.sysname, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        print_info_item("主机名称:", info.nodename, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        print_info_item("内核版本:", info.release, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        print_info_item("系统版本:", info.version, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        print_info_item("硬件架构:", info.machine, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    }
    
    // 发行版信息（简化）
    FILE *fp = fopen("/etc/os-release", "r");
    if (fp != NULL) {
        char line[256];
        char distro_name[256] = "Unknown";
        char distro_version[256] = "";
        
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "PRETTY_NAME=") != NULL) {
                char *start = strchr(line, '"');
                if (start != NULL) {
                    char *end = strchr(start + 1, '"');
                    if (end != NULL) {
                        *end = '\0';
                        strncpy(distro_name, start + 1, sizeof(distro_name) - 1);
                    }
                }
            } else if (strstr(line, "VERSION_ID=") != NULL) {
                char *start = strchr(line, '"');
                if (start != NULL) {
                    char *end = strchr(start + 1, '"');
                    if (end != NULL) {
                        *end = '\0';
                        strncpy(distro_version, start + 1, sizeof(distro_version) - 1);
                    }
                }
            }
        }
        fclose(fp);
        
        if (strlen(distro_version) > 0) {
            char distro_full[512];
            snprintf(distro_full, sizeof(distro_full), "%s %s", distro_name, distro_version);
            print_info_item("发行版本:", distro_full, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        } else {
            print_info_item("发行版本:", distro_name, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        }
    }
    
    printf("\n");
}

// 显示简要信息
static void show_brief_info(Options *opts) {
    // 显示标题
    if (opts->color) {
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_MAGENTA, "                      🖥️  系统信息概览");
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
        printf("\n");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
        printf("                      系统信息概览\n");
        printf("══════════════════════════════════════════════════════════════\n\n");
    }
    
    // 系统信息
    struct utsname info;
    if (uname(&info) == 0) {
        color_print(COLOR_BRIGHT_CYAN, "系统: ");
        printf("%s %s (%s)\n", info.sysname, info.release, info.machine);
    }
    
    // CPU信息
    int cores = get_cpu_core_count();
    char *model = get_cpu_model();
    color_print(COLOR_BRIGHT_CYAN, "CPU:  ");
    printf("%s (%d核心)\n", model, cores);
    
    // 内存信息
    unsigned long total_kb, free_kb, available_kb, used_kb;
    get_memory_info(&total_kb, &free_kb, &available_kb, &used_kb);
    char *total_str = format_size(total_kb * 1024);
    char *used_str = format_size(used_kb * 1024);
    float usage_percent = total_kb > 0 ? (float)used_kb / total_kb * 100.0f : 0;
    
    color_print(COLOR_BRIGHT_CYAN, "内存: ");
    printf("%s / %s (%.1f%%)\n", used_str, total_str, usage_percent);
    
    // 磁盘信息
    unsigned long total_bytes, free_bytes, used_bytes;
    float disk_usage;
    get_disk_info(&total_bytes, &free_bytes, &used_bytes, &disk_usage);
    char *disk_total_str = format_size(total_bytes);
    char *disk_used_str = format_size(used_bytes);
    
    color_print(COLOR_BRIGHT_CYAN, "磁盘: ");
    printf("%s / %s (%.1f%%)\n", disk_used_str, disk_total_str, disk_usage);
    
    // 运行时间
    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        long uptime_seconds = sys_info.uptime;
        long days = uptime_seconds / 86400;
        long hours = (uptime_seconds % 86400) / 3600;
        
        color_print(COLOR_BRIGHT_CYAN, "运行: ");
        if (days > 0) {
            printf("%ld天%ld小时", days, hours);
        } else {
            printf("%ld小时", hours);
        }
        
        // 负载
        printf("  负载: %.2f, %.2f, %.2f\n", 
               sys_info.loads[0] / 65536.0,
               sys_info.loads[1] / 65536.0,
               sys_info.loads[2] / 65536.0);
    }
    
    // 用户信息
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL) {
        color_print(COLOR_BRIGHT_CYAN, "用户: ");
        printf("%s@", pw->pw_name);
        
        // 主机名
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            printf("%s", hostname);
        }
        printf("\n");
    }
    
    printf("\n");
    if (opts->color) {
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_YELLOW, "使用 'tkinfo --help' 查看更多选项");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
        printf("使用 'tkinfo --help' 查看更多选项\n");
    }
}

// tkinfo主函数
int tkinfo_main(int argc, char **argv) {
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
    
    // 设置颜色
    if (opts.color) {
        enable_color();
    } else {
        disable_color();
    }
    
    // 显示信息
    if (opts.brief) {
        show_brief_info(&opts);
    } else {
        // 显示详细分类信息
        if (opts.color) {
            color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
            color_println(COLOR_BRIGHT_MAGENTA, "                      📊 系统详细信息");
            color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
            printf("\n");
        } else {
            printf("══════════════════════════════════════════════════════════════\n");
            printf("                      系统详细信息\n");
            printf("══════════════════════════════════════════════════════════════\n\n");
        }
        
        if (opts.os_info || opts.detailed) show_os_info(&opts);
        if (opts.cpu_info || opts.detailed) show_cpu_info(&opts);
        if (opts.memory_info || opts.detailed) show_memory_info(&opts);
        if (opts.disk_info || opts.detailed) show_disk_info(&opts);
        if (opts.uptime_info || opts.detailed) show_uptime_info(&opts);
        if (opts.user_info || opts.detailed) show_user_info(&opts);
        if (opts.process_info || opts.detailed) show_process_info(&opts);
        if (opts.network_info || opts.detailed) show_network_info(&opts);
        
        if (opts.color) {
            color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
        } else {
            printf("══════════════════════════════════════════════════════════════\n");
        }
    }
    
    return 0;
}