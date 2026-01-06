// src/system_tools/tkhw.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include <ctype.h>
#include <pci/pci.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 硬件信息结构
typedef struct {
    // CPU信息
    char cpu_vendor[64];
    char cpu_model[128];
    int cpu_cores;
    int cpu_threads;
    float cpu_freq;
    
    // 内存信息
    unsigned long total_memory;
    unsigned long free_memory;
    int memory_slots;
    
    // 磁盘信息
    char disk_model[128];
    unsigned long disk_size;
    char disk_type[32];
    
    // GPU信息
    char gpu_vendor[64];
    char gpu_model[128];
    int gpu_memory;
    
    // 主板信息
    char motherboard[128];
    char bios_version[64];
    
    // 网络信息
    char network_cards[256];
    
    // 其他
    char hostname[64];
    char kernel_version[64];
    char architecture[32];
} HardwareInfo;

// 选项结构
typedef struct {
    int show_all;       // 显示所有信息
    int show_cpu;       // 显示CPU信息
    int show_memory;    // 显示内存信息
    int show_disk;      // 显示磁盘信息
    int show_gpu;       // 显示GPU信息
    int show_network;   // 显示网络信息
    int show_motherboard; // 显示主板信息
    int simple;         // 简单模式
    int color;          // 彩色输出
    int help;           // 帮助
    int version;        // 版本
} Options;

// 初始化选项
static void init_options(Options *opts) {
    opts->show_all = 0;
    opts->show_cpu = 0;
    opts->show_memory = 0;
    opts->show_disk = 0;
    opts->show_gpu = 0;
    opts->show_network = 0;
    opts->show_motherboard = 0;
    opts->simple = 0;
    opts->color = is_color_supported();
    opts->help = 0;
    opts->version = 0;
}

// 显示帮助
static void show_help() {
    printf("tkhw - 硬件信息检测工具\n");
    printf("用法: tkhw [选项]\n");
    printf("选项:\n");
    printf("  -a, --all          显示所有硬件信息\n");
    printf("  -c, --cpu          显示CPU信息\n");
    printf("  -m, --memory       显示内存信息\n");
    printf("  -d, --disk         显示磁盘信息\n");
    printf("  -g, --gpu          显示GPU信息\n");
    printf("  -n, --network      显示网络信息\n");
    printf("  -b, --motherboard  显示主板信息\n");
    printf("  -s, --simple       简单模式\n");
    printf("      --no-color     无颜色输出\n");
    printf("      --help         显示帮助\n");
    printf("      --version      显示版本\n");
}

// 显示版本
static void show_version() {
    printf("tkhw v1.0.0 - TermKit 硬件信息工具\n");
}

// 解析选项
static int parse_options(int argc, char **argv, Options *opts) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            opts->show_all = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cpu") == 0) {
            opts->show_cpu = 1;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--memory") == 0) {
            opts->show_memory = 1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--disk") == 0) {
            opts->show_disk = 1;
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gpu") == 0) {
            opts->show_gpu = 1;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--network") == 0) {
            opts->show_network = 1;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--motherboard") == 0) {
            opts->show_motherboard = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--simple") == 0) {
            opts->simple = 1;
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
            return -1;
        }
    }
    
    // 如果没有指定任何显示选项，默认显示所有
    if (!opts->show_all && !opts->show_cpu && !opts->show_memory && 
        !opts->show_disk && !opts->show_gpu && !opts->show_network && 
        !opts->show_motherboard) {
        opts->show_all = 1;
    }
    
    return 1;
}

// 获取CPU信息
static void get_cpu_info(HardwareInfo *info) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) return;
    
    char line[256];
    int processor_count = 0;
    int core_count = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "vendor_id")) {
            char *colon = strchr(line, ':');
            if (colon) {
                strncpy(info->cpu_vendor, colon + 2, sizeof(info->cpu_vendor) - 1);
                char *newline = strchr(info->cpu_vendor, '\n');
                if (newline) *newline = '\0';
            }
        } else if (strstr(line, "model name")) {
            char *colon = strchr(line, ':');
            if (colon) {
                strncpy(info->cpu_model, colon + 2, sizeof(info->cpu_model) - 1);
                char *newline = strchr(info->cpu_model, '\n');
                if (newline) *newline = '\0';
            }
        } else if (strstr(line, "processor")) {
            processor_count++;
        } else if (strstr(line, "cpu cores")) {
            char *colon = strchr(line, ':');
            if (colon) core_count = atoi(colon + 2);
        } else if (strstr(line, "cpu MHz")) {
            char *colon = strchr(line, ':');
            if (colon) info->cpu_freq = atof(colon + 2) / 1000.0;
        }
    }
    
    fclose(fp);
    
    info->cpu_cores = core_count > 0 ? core_count : processor_count;
    info->cpu_threads = processor_count;
}

// 获取内存信息
static void get_memory_info(HardwareInfo *info) {
    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        info->total_memory = sys_info.totalram * sys_info.mem_unit;
        info->free_memory = sys_info.freeram * sys_info.mem_unit;
    }
    
    // 尝试获取内存插槽信息
    info->memory_slots = 0;
    DIR *dir = opendir("/sys/devices/system/edac/mc");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                info->memory_slots++;
            }
        }
        closedir(dir);
    }
    
    if (info->memory_slots == 0) {
        info->memory_slots = 2; // 默认值
    }
}

// 获取磁盘信息
static void get_disk_info(HardwareInfo *info) {
    FILE *fp = popen("lsblk -d -o MODEL,SIZE,TYPE 2>/dev/null | head -2 | tail -1", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            char model[128], size[64], type[32];
            if (sscanf(line, "%127s %63s %31s", model, size, type) == 3) {
                strncpy(info->disk_model, model, sizeof(info->disk_model) - 1);
                strncpy(info->disk_type, type, sizeof(info->disk_type) - 1);
                
                // 转换大小
                if (strstr(size, "G")) {
                    info->disk_size = atof(size) * 1024 * 1024 * 1024;
                } else if (strstr(size, "T")) {
                    info->disk_size = atof(size) * 1024 * 1024 * 1024 * 1024;
                }
            }
        }
        pclose(fp);
    }
    
    // 如果lsblk失败，使用df
    if (strlen(info->disk_model) == 0) {
        struct statvfs buf;
        if (statvfs("/", &buf) == 0) {
            info->disk_size = buf.f_blocks * buf.f_frsize;
            strcpy(info->disk_type, "Unknown");
            strcpy(info->disk_model, "Unknown");
        }
    }
}

// 获取GPU信息
static void get_gpu_info(HardwareInfo *info) {
    FILE *fp = popen("lspci | grep -i vga 2>/dev/null | head -1", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            char *colon = strchr(line, ':');
            if (colon) {
                strncpy(info->gpu_model, colon + 2, sizeof(info->gpu_model) - 1);
                char *newline = strchr(info->gpu_model, '\n');
                if (newline) *newline = '\0';
                
                // 提取厂商
                if (strstr(line, "Intel")) strcpy(info->gpu_vendor, "Intel");
                else if (strstr(line, "NVIDIA")) strcpy(info->gpu_vendor, "NVIDIA");
                else if (strstr(line, "AMD")) strcpy(info->gpu_vendor, "AMD");
                else if (strstr(line, "ATI")) strcpy(info->gpu_vendor, "ATI");
                else strcpy(info->gpu_vendor, "Unknown");
            }
        }
        pclose(fp);
    }
    
    // GPU显存（简化）
    info->gpu_memory = 0;
}

// 获取主板信息
static void get_motherboard_info(HardwareInfo *info) {
    FILE *fp = popen("dmidecode -t baseboard 2>/dev/null | grep 'Product Name' | head -1", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            char *colon = strchr(line, ':');
            if (colon) {
                strncpy(info->motherboard, colon + 2, sizeof(info->motherboard) - 1);
                char *newline = strchr(info->motherboard, '\n');
                if (newline) *newline = '\0';
            }
        }
        pclose(fp);
    }
    
    // BIOS版本
    fp = popen("dmidecode -t bios 2>/dev/null | grep 'Version' | head -1", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            char *colon = strchr(line, ':');
            if (colon) {
                strncpy(info->bios_version, colon + 2, sizeof(info->bios_version) - 1);
                char *newline = strchr(info->bios_version, '\n');
                if (newline) *newline = '\0';
            }
        }
        pclose(fp);
    }
}

// 获取网络信息
static void get_network_info(HardwareInfo *info) {
    FILE *fp = popen("lspci | grep -i network 2>/dev/null | head -3", "r");
    if (fp) {
        char line[256];
        info->network_cards[0] = '\0';
        while (fgets(line, sizeof(line), fp)) {
            char *colon = strchr(line, ':');
            if (colon) {
                strcat(info->network_cards, colon + 2);
                char *newline = strchr(info->network_cards, '\n');
                if (newline) *newline = ';';
            }
        }
        pclose(fp);
    }
    
    if (strlen(info->network_cards) == 0) {
        strcpy(info->network_cards, "Unknown");
    }
}

// 获取主机名和架构
static void get_system_info(HardwareInfo *info) {
    // 主机名
    gethostname(info->hostname, sizeof(info->hostname) - 1);
    
    // 内核版本和架构
    struct utsname uts;
    if (uname(&uts) == 0) {
        strncpy(info->kernel_version, uts.release, sizeof(info->kernel_version) - 1);
        strncpy(info->architecture, uts.machine, sizeof(info->architecture) - 1);
    }
}

// 收集所有硬件信息
static void collect_hardware_info(HardwareInfo *info) {
    memset(info, 0, sizeof(HardwareInfo));
    
    get_cpu_info(info);
    get_memory_info(info);
    get_disk_info(info);
    get_gpu_info(info);
    get_motherboard_info(info);
    get_network_info(info);
    get_system_info(info);
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
        color_print(color, "  %-20s", label);
        printf("%s\n", value);
    } else {
        printf("  %-20s%s\n", label, value);
    }
}

// 显示简单模式
static void show_simple_info(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_CYAN, "                    硬件信息概览");
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
        printf("                    硬件信息概览\n");
        printf("══════════════════════════════════════════════════════════════\n");
    }
    
    printf("\n");
    
    // CPU信息
    if (opts->color) color_print(COLOR_BRIGHT_GREEN, "💻 CPU: ");
    else printf("CPU: ");
    printf("%s %s (%d核心/%d线程)\n", info->cpu_vendor, info->cpu_model, 
           info->cpu_cores, info->cpu_threads);
    
    // 内存信息
    char *mem_str = format_size(info->total_memory);
    if (opts->color) color_print(COLOR_BRIGHT_GREEN, "💾 内存: ");
    else printf("内存: ");
    printf("%s (%d插槽)\n", mem_str, info->memory_slots);
    
    // 磁盘信息
    char *disk_str = format_size(info->disk_size);
    if (opts->color) color_print(COLOR_BRIGHT_GREEN, "💽 磁盘: ");
    else printf("磁盘: ");
    printf("%s %s\n", info->disk_model, disk_str);
    
    // GPU信息
    if (strlen(info->gpu_model) > 0 && strcmp(info->gpu_model, "Unknown") != 0) {
        if (opts->color) color_print(COLOR_BRIGHT_GREEN, "🎮 GPU: ");
        else printf("GPU: ");
        printf("%s %s\n", info->gpu_vendor, info->gpu_model);
    }
    
    // 主板信息
    if (strlen(info->motherboard) > 0 && strcmp(info->motherboard, "Unknown") != 0) {
        if (opts->color) color_print(COLOR_BRIGHT_GREEN, "🖥️  主板: ");
        else printf("主板: ");
        printf("%s (BIOS: %s)\n", info->motherboard, info->bios_version);
    }
    
    // 系统信息
    if (opts->color) color_print(COLOR_BRIGHT_GREEN, "🐧 系统: ");
    else printf("系统: ");
    printf("%s (%s) %s\n", info->hostname, info->architecture, info->kernel_version);
    
    printf("\n");
    
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
    }
}

// 显示详细CPU信息
static void show_cpu_info_detailed(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "💻 CPU信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("CPU信息:\n");
        print_separator(NULL);
    }
    
    print_info_item("厂商:", info->cpu_vendor, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("型号:", info->cpu_model, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    char cores_str[32];
    snprintf(cores_str, sizeof(cores_str), "%d 核心 / %d 线程", 
             info->cpu_cores, info->cpu_threads);
    print_info_item("核心/线程:", cores_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    if (info->cpu_freq > 0) {
        char freq_str[32];
        snprintf(freq_str, sizeof(freq_str), "%.2f GHz", info->cpu_freq);
        print_info_item("频率:", freq_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    }
    
    printf("\n");
}

// 显示详细内存信息
static void show_memory_info_detailed(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "💾 内存信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("内存信息:\n");
        print_separator(NULL);
    }
    
    char *total_str = format_size(info->total_memory);
    char *free_str = format_size(info->free_memory);
    unsigned long used = info->total_memory - info->free_memory;
    char *used_str = format_size(used);
    
    print_info_item("总内存:", total_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("已使用:", used_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("空闲内存:", free_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    char slots_str[32];
    snprintf(slots_str, sizeof(slots_str), "%d", info->memory_slots);
    print_info_item("内存插槽:", slots_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    if (info->total_memory > 0) {
        float usage = (float)used / info->total_memory * 100.0;
        char usage_str[32];
        snprintf(usage_str, sizeof(usage_str), "%.1f%%", usage);
        print_info_item("使用率:", usage_str, opts->color ? 
                       (usage > 80 ? COLOR_BRIGHT_RED : 
                        usage > 60 ? COLOR_BRIGHT_YELLOW : 
                        COLOR_BRIGHT_GREEN) : NULL);
    }
    
    printf("\n");
}

// 显示详细磁盘信息
static void show_disk_info_detailed(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "💽 磁盘信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("磁盘信息:\n");
        print_separator(NULL);
    }
    
    print_info_item("型号:", info->disk_model, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    char *size_str = format_size(info->disk_size);
    print_info_item("容量:", size_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    print_info_item("类型:", info->disk_type, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    // 获取磁盘使用情况
    struct statvfs buf;
    if (statvfs("/", &buf) == 0) {
        unsigned long total = buf.f_blocks * buf.f_frsize;
        unsigned long free = buf.f_bfree * buf.f_frsize;
        unsigned long used = total - free;
        
        char *disk_total = format_size(total);
        char *disk_used = format_size(used);
        char *disk_free = format_size(free);
        
        print_info_item("总空间:", disk_total, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        print_info_item("已使用:", disk_used, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        print_info_item("可用空间:", disk_free, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        
        if (total > 0) {
            float usage = (float)used / total * 100.0;
            char usage_str[32];
            snprintf(usage_str, sizeof(usage_str), "%.1f%%", usage);
            print_info_item("使用率:", usage_str, opts->color ? 
                           (usage > 90 ? COLOR_BRIGHT_RED : 
                            usage > 80 ? COLOR_BRIGHT_YELLOW : 
                            COLOR_BRIGHT_GREEN) : NULL);
        }
    }
    
    printf("\n");
}

// 显示详细GPU信息
static void show_gpu_info_detailed(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🎮 GPU信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("GPU信息:\n");
        print_separator(NULL);
    }
    
    print_info_item("厂商:", info->gpu_vendor, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("型号:", info->gpu_model, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    if (info->gpu_memory > 0) {
        char *mem_str = format_size(info->gpu_memory);
        print_info_item("显存:", mem_str, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    }
    
    printf("\n");
}

// 显示详细主板信息
static void show_motherboard_info_detailed(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🖥️  主板信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("主板信息:\n");
        print_separator(NULL);
    }
    
    print_info_item("型号:", info->motherboard, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("BIOS版本:", info->bios_version, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    printf("\n");
}

// 显示详细网络信息
static void show_network_info_detailed(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🌐 网络适配器:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("网络适配器:\n");
        print_separator(NULL);
    }
    
    // 分割网络卡信息
    char *cards = strtok(info->network_cards, ";");
    int count = 0;
    
    while (cards != NULL) {
        char label[32];
        snprintf(label, sizeof(label), "网卡%d:", ++count);
        print_info_item(label, cards, opts->color ? COLOR_BRIGHT_GREEN : NULL);
        cards = strtok(NULL, ";");
    }
    
    if (count == 0) {
        print_info_item("网卡:", "未检测到网络适配器", opts->color ? COLOR_BRIGHT_YELLOW : NULL);
    }
    
    printf("\n");
}

// 显示系统信息
static void show_system_info_detailed(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🐧 系统信息:");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("系统信息:\n");
        print_separator(NULL);
    }
    
    print_info_item("主机名:", info->hostname, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("架构:", info->architecture, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    print_info_item("内核版本:", info->kernel_version, opts->color ? COLOR_BRIGHT_GREEN : NULL);
    
    printf("\n");
}

// 显示详细模式
static void show_detailed_info(HardwareInfo *info, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
        color_println(COLOR_BRIGHT_MAGENTA, "                   详细硬件信息");
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
        printf("\n");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
        printf("                   详细硬件信息\n");
        printf("══════════════════════════════════════════════════════════════\n\n");
    }
    
    if (opts->show_all || opts->show_cpu) show_cpu_info_detailed(info, opts);
    if (opts->show_all || opts->show_memory) show_memory_info_detailed(info, opts);
    if (opts->show_all || opts->show_disk) show_disk_info_detailed(info, opts);
    if (opts->show_all || opts->show_gpu) show_gpu_info_detailed(info, opts);
    if (opts->show_all || opts->show_motherboard) show_motherboard_info_detailed(info, opts);
    if (opts->show_all || opts->show_network) show_network_info_detailed(info, opts);
    
    // 总是显示系统信息
    show_system_info_detailed(info, opts);
    
    if (opts->color) {
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
    }
}

// tkhw主函数
int tkhw_main(int argc, char **argv) {
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
    
    HardwareInfo info;
    collect_hardware_info(&info);
    
    if (opts.simple) {
        show_simple_info(&info, &opts);
    } else {
        show_detailed_info(&info, &opts);
    }
    
    return 0;
}