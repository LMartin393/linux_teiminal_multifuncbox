// src/system_tools/tkmon.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <signal.h>
#include <ncurses.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 监控数据
typedef struct {
    // CPU使用率
    unsigned long long last_total;
    unsigned long long last_idle;
    float cpu_usage;
    
    // 内存信息
    unsigned long mem_total;
    unsigned long mem_used;
    unsigned long mem_free;
    float mem_usage;
    
    // 交换空间
    unsigned long swap_total;
    unsigned long swap_used;
    unsigned long swap_free;
    float swap_usage;
    
    // 磁盘信息
    unsigned long disk_total;
    unsigned long disk_used;
    unsigned long disk_free;
    float disk_usage;
    
    // 系统负载
    float load_1;
    float load_5;
    float load_15;
    
    // 运行时间
    long uptime;
    
    // 进程数
    int process_count;
    
    // 更新时间
    time_t update_time;
} MonitorData;

// 选项
typedef struct {
    int interval;       // 更新间隔（秒）
    int simple_mode;    // 简单模式
    int no_color;       // 无颜色
    int help;          // 帮助
    int version;       // 版本
} Options;

// 初始化选项
static void init_options(Options *opts) {
    opts->interval = 2;
    opts->simple_mode = 0;
    opts->no_color = 0;
    opts->help = 0;
    opts->version = 0;
}

// 显示帮助
static void show_help(void) {
    printf("tkmon - 实时系统监控\n");
    printf("用法: tkmon [选项]\n");
    printf("选项:\n");
    printf("  -i SECONDS  更新间隔（默认: 2秒）\n");
    printf("  -s          简单模式\n");
    printf("  --no-color  无颜色输出\n");
    printf("  --help      显示帮助\n");
    printf("  --version   显示版本\n");
}

// 显示版本
static void show_version(void) {
    printf("tkmon v1.0.0 - TermKit 系统监控工具\n");
}

// 解析选项
static int parse_options(int argc, char **argv, Options *opts) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 < argc) {
                opts->interval = atoi(argv[++i]);
                if (opts->interval < 1) opts->interval = 1;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
            opts->simple_mode = 1;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            opts->no_color = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            opts->help = 1;
            return 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            opts->version = 1;
            return 1;
        }
    }
    return 1;
}

// 获取CPU使用率
static float get_cpu_usage(MonitorData *data) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    
    char line[256];
    unsigned long long total = 0, idle = 0;
    
    if (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            unsigned long long user, nice, system, idle_t, iowait, irq, softirq;
            sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle_t, &iowait, &irq, &softirq);
            
            idle = idle_t + iowait;
            total = user + nice + system + idle + irq + softirq;
        }
    }
    
    fclose(fp);
    
    if (data->last_total > 0 && data->last_idle > 0) {
        unsigned long long total_diff = total - data->last_total;
        unsigned long long idle_diff = idle - data->last_idle;
        
        if (total_diff > 0) {
            data->cpu_usage = 100.0 * (total_diff - idle_diff) / total_diff;
        }
    }
    
    data->last_total = total;
    data->last_idle = idle;
    
    return data->cpu_usage;
}

// 获取内存信息
static void get_memory_info(MonitorData *data) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return;
    
    char line[256];
    data->mem_total = data->mem_free = data->mem_used = 0;
    data->swap_total = data->swap_free = data->swap_used = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %lu kB", &data->mem_total);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line, "MemFree: %lu kB", &data->mem_free);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            unsigned long available;
            sscanf(line, "MemAvailable: %lu kB", &available);
            data->mem_used = data->mem_total - available;
        } else if (strncmp(line, "SwapTotal:", 10) == 0) {
            sscanf(line, "SwapTotal: %lu kB", &data->swap_total);
        } else if (strncmp(line, "SwapFree:", 9) == 0) {
            sscanf(line, "SwapFree: %lu kB", &data->swap_free);
        }
    }
    
    fclose(fp);
    
    if (data->mem_total > 0) {
        data->mem_usage = 100.0 * data->mem_used / data->mem_total;
        data->swap_used = data->swap_total - data->swap_free;
        if (data->swap_total > 0) {
            data->swap_usage = 100.0 * data->swap_used / data->swap_total;
        }
    }
}

// 获取磁盘信息
static void get_disk_info(MonitorData *data) {
    struct statvfs buf;
    if (statvfs("/", &buf) == 0) {
        data->disk_total = buf.f_blocks * buf.f_frsize;
        data->disk_free = buf.f_bfree * buf.f_frsize;
        data->disk_used = data->disk_total - data->disk_free;
        
        if (data->disk_total > 0) {
            data->disk_usage = 100.0 * data->disk_used / data->disk_total;
        }
    }
}

// 获取系统负载
static void get_load_average(MonitorData *data) {
    FILE *fp = fopen("/proc/loadavg", "r");
    if (fp) {
        fscanf(fp, "%f %f %f", &data->load_1, &data->load_5, &data->load_15);
        fclose(fp);
    }
}

// 获取进程数
static int get_process_count(void) {
    DIR *dir = opendir("/proc");
    if (!dir) return 0;
    
    int count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            count++;
        }
    }
    
    closedir(dir);
    return count;
}

// 获取运行时间
static long get_uptime(void) {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return info.uptime;
    }
    return 0;
}

// 更新所有监控数据
static void update_monitor_data(MonitorData *data) {
    get_cpu_usage(data);
    get_memory_info(data);
    get_disk_info(data);
    get_load_average(data);
    data->process_count = get_process_count();
    data->uptime = get_uptime();
    data->update_time = time(NULL);
}

// 显示进度条
static void print_progress_bar(float percentage, int width, int no_color) {
    int filled = (int)(width * percentage / 100.0);
    if (filled > width) filled = width;
    
    printf("[");
    
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            if (!no_color) {
                if (percentage > 80) printf("%s#", COLOR_BRIGHT_RED);
                else if (percentage > 60) printf("%s#", COLOR_BRIGHT_YELLOW);
                else printf("%s#", COLOR_BRIGHT_GREEN);
            } else {
                printf("#");
            }
        } else {
            printf(" ");
        }
    }
    
    if (!no_color) printf("%s]", COLOR_RESET);
    else printf("]");
    
    printf(" %5.1f%%", percentage);
}

// 显示简单模式
static void show_simple_mode(MonitorData *data, Options *opts) {
    printf("\033[2J\033[H"); // 清屏
    
    if (!opts->no_color) {
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_CYAN, "                tkmon - 系统监控");
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════\n");
        printf("                tkmon - 系统监控\n");
        printf("══════════════════════════════════════════════════\n");
    }
    
    printf("\n");
    
    // CPU使用率
    if (!opts->no_color) color_print(COLOR_BRIGHT_GREEN, "CPU使用率:  ");
    else printf("CPU使用率:  ");
    print_progress_bar(data->cpu_usage, 30, opts->no_color);
    printf("\n");
    
    // 内存使用率
    if (!opts->no_color) color_print(COLOR_BRIGHT_GREEN, "内存使用率: ");
    else printf("内存使用率: ");
    print_progress_bar(data->mem_usage, 30, opts->no_color);
    printf("\n");
    
    // 磁盘使用率
    if (!opts->no_color) color_print(COLOR_BRIGHT_GREEN, "磁盘使用率: ");
    else printf("磁盘使用率: ");
    print_progress_bar(data->disk_usage, 30, opts->no_color);
    printf("\n");
    
    printf("\n");
    
    // 系统负载
    if (!opts->no_color) {
        color_print(COLOR_BRIGHT_YELLOW, "系统负载: ");
        printf("%.2f, %.2f, %.2f", data->load_1, data->load_5, data->load_15);
    } else {
        printf("系统负载: %.2f, %.2f, %.2f", data->load_1, data->load_5, data->load_15);
    }
    
    // 进程数
    if (!opts->no_color) {
        color_print(COLOR_BRIGHT_YELLOW, "   进程数: ");
        printf("%d", data->process_count);
    } else {
        printf("   进程数: %d", data->process_count);
    }
    
    printf("\n");
    
    // 运行时间
    long days = data->uptime / 86400;
    long hours = (data->uptime % 86400) / 3600;
    long minutes = (data->uptime % 3600) / 60;
    
    if (!opts->no_color) {
        color_print(COLOR_BRIGHT_YELLOW, "运行时间: ");
        if (days > 0) printf("%ld天 ", days);
        printf("%02ld:%02ld:%02ld", hours, minutes, data->uptime % 60);
    } else {
        printf("运行时间: ");
        if (days > 0) printf("%ld天 ", days);
        printf("%02ld:%02ld:%02ld", hours, minutes, data->uptime % 60);
    }
    
    printf("\n\n");
    
    if (!opts->no_color) {
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_YELLOW, "按 Ctrl+C 退出监控");
    } else {
        printf("══════════════════════════════════════════════════\n");
        printf("按 Ctrl+C 退出监控\n");
    }
}

// 显示详细模式
static void show_detailed_mode(MonitorData *data, Options *opts) {
    printf("\033[2J\033[H"); // 清屏
    
    if (!opts->no_color) {
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_CYAN, "                    tkmon - 详细系统监控");
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
        printf("                    tkmon - 详细系统监控\n");
        printf("══════════════════════════════════════════════════════════════\n");
    }
    
    printf("\n");
    
    // CPU信息
    if (!opts->no_color) color_println(COLOR_BRIGHT_GREEN, "CPU信息:");
    else printf("CPU信息:\n");
    
    printf("  使用率: ");
    print_progress_bar(data->cpu_usage, 40, opts->no_color);
    printf("\n");
    
    // 内存信息
    if (!opts->no_color) color_println(COLOR_BRIGHT_GREEN, "\n内存信息:");
    else printf("\n内存信息:\n");
    
    char *mem_total = format_size(data->mem_total * 1024);
    char *mem_used = format_size(data->mem_used * 1024);
    char *mem_free = format_size(data->mem_free * 1024);
    
    printf("  总量: %s  已用: %s  空闲: %s\n", mem_total, mem_used, mem_free);
    printf("  使用率: ");
    print_progress_bar(data->mem_usage, 40, opts->no_color);
    printf("\n");
    
    // 交换空间
    if (data->swap_total > 0) {
        char *swap_total = format_size(data->swap_total * 1024);
        char *swap_used = format_size(data->swap_used * 1024);
        
        printf("  交换空间: %s / %s (%.1f%%)\n", swap_used, swap_total, data->swap_usage);
    }
    
    // 磁盘信息
    if (!opts->no_color) color_println(COLOR_BRIGHT_GREEN, "\n磁盘信息:");
    else printf("\n磁盘信息:\n");
    
    char *disk_total = format_size(data->disk_total);
    char *disk_used = format_size(data->disk_used);
    char *disk_free = format_size(data->disk_free);
    
    printf("  总量: %s  已用: %s  空闲: %s\n", disk_total, disk_used, disk_free);
    printf("  使用率: ");
    print_progress_bar(data->disk_usage, 40, opts->no_color);
    printf("\n");
    
    // 系统信息
    if (!opts->no_color) color_println(COLOR_BRIGHT_GREEN, "\n系统信息:");
    else printf("\n系统信息:\n");
    
    printf("  系统负载: %.2f (1分钟), %.2f (5分钟), %.2f (15分钟)\n", 
           data->load_1, data->load_5, data->load_15);
    printf("  进程总数: %d\n", data->process_count);
    
    // 运行时间
    long days = data->uptime / 86400;
    long hours = (data->uptime % 86400) / 3600;
    long minutes = (data->uptime % 3600) / 60;
    long seconds = data->uptime % 60;
    
    printf("  运行时间: ");
    if (days > 0) printf("%ld天 ", days);
    printf("%02ld:%02ld:%02ld\n", hours, minutes, seconds);
    
    // 更新时间
    char time_str[64];
    struct tm *tm_info = localtime(&data->update_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("  更新时间: %s\n", time_str);
    
    printf("\n");
    
    if (!opts->no_color) {
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_YELLOW, "按 Ctrl+C 退出监控");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
        printf("按 Ctrl+C 退出监控\n");
    }
}

// 信号处理
static volatile int running = 1;

static void signal_handler(int sig) {
    running = 0;
}

// tkmon主函数
int tkmon_main(int argc, char **argv) {
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
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 初始化监控数据
    MonitorData data = {0};
    update_monitor_data(&data);
    
    // 隐藏光标
    printf("\033[?25l");
    
    // 主循环
    while (running) {
        update_monitor_data(&data);
        
        if (opts.simple_mode) {
            show_simple_mode(&data, &opts);
        } else {
            show_detailed_mode(&data, &opts);
        }
        
        // 等待
        sleep(opts.interval);
    }
    
    // 显示光标
    printf("\033[?25h");
    
    // 清屏并退出
    printf("\033[2J\033[H");
    printf("监控已停止\n");
    
    return 0;
}