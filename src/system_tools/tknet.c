// src/system_tools/tknet.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <time.h>
#include <errno.h>
#include "../common/colors.h"
#include "../common/utils.h"

// 网络接口信息
typedef struct {
    char name[16];          // 接口名
    char ip_addr[INET_ADDRSTRLEN];    // IPv4地址
    char netmask[INET_ADDRSTRLEN];    // 子网掩码
    char broadcast[INET_ADDRSTRLEN];  // 广播地址
    char mac_addr[18];      // MAC地址
    unsigned long rx_bytes; // 接收字节数
    unsigned long tx_bytes; // 发送字节数
    int is_up;              // 是否启用
    int is_loopback;        // 是否回环接口
} InterfaceInfo;

// 网络连接信息
typedef struct {
    char protocol[16];      // 协议类型
    char local_addr[46];    // 本地地址
    char foreign_addr[46];  // 远程地址
    char state[16];         // 连接状态
    int local_port;         // 本地端口
    int foreign_port;       // 远程端口
    int pid;                // 进程ID
    char program[64];       // 程序名
} ConnectionInfo;

// 选项结构
typedef struct {
    int show_interfaces;    // 显示网络接口
    int show_connections;   // 显示网络连接
    int show_routing;       // 显示路由表
    int show_arp;          // 显示ARP表
    int show_dns;          // 显示DNS信息
    int show_stats;        // 显示统计信息
    int tcp_only;          // 只显示TCP连接
    int udp_only;          // 只显示UDP连接
    int listening_only;    // 只显示监听端口
    int numeric;           // 显示数字地址
    int continuous;        // 持续监控
    int refresh_interval;  // 刷新间隔
    int color;             // 彩色输出
    int help;              // 帮助
    int version;           // 版本
} Options;

// 初始化选项
static void init_options(Options *opts) {
    opts->show_interfaces = 0;
    opts->show_connections = 0;
    opts->show_routing = 0;
    opts->show_arp = 0;
    opts->show_dns = 0;
    opts->show_stats = 0;
    opts->tcp_only = 0;
    opts->udp_only = 0;
    opts->listening_only = 0;
    opts->numeric = 0;
    opts->continuous = 0;
    opts->refresh_interval = 2;
    opts->color = is_color_supported();
    opts->help = 0;
    opts->version = 0;
}

// 显示帮助
static void show_help() {
    printf("tknet - 网络状态查看工具\n");
    printf("用法: tknet [选项]\n");
    printf("选项:\n");
    printf("  -i, --interfaces    显示网络接口信息\n");
    printf("  -c, --connections   显示网络连接信息\n");
    printf("  -r, --route         显示路由表\n");
    printf("  -a, --arp           显示ARP表\n");
    printf("  -d, --dns           显示DNS信息\n");
    printf("  -s, --stats         显示网络统计\n");
    printf("  -t, --tcp           只显示TCP连接\n");
    printf("  -u, --udp           只显示UDP连接\n");
    printf("  -l, --listen        只显示监听端口\n");
    printf("  -n, --numeric       显示数字地址\n");
    printf("  -C, --continuous    持续监控模式\n");
    printf("      --interval SEC  监控间隔（默认: 2秒）\n");
    printf("      --no-color      无颜色输出\n");
    printf("      --help          显示帮助\n");
    printf("      --version       显示版本\n");
}

// 显示版本
static void show_version() {
    printf("tknet v1.0.0 - TermKit 网络工具\n");
}

// 解析选项
static int parse_options(int argc, char **argv, Options *opts) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interfaces") == 0) {
            opts->show_interfaces = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--connections") == 0) {
            opts->show_connections = 1;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--route") == 0) {
            opts->show_routing = 1;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--arp") == 0) {
            opts->show_arp = 1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dns") == 0) {
            opts->show_dns = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0) {
            opts->show_stats = 1;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tcp") == 0) {
            opts->tcp_only = 1;
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--udp") == 0) {
            opts->udp_only = 1;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--listen") == 0) {
            opts->listening_only = 1;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--numeric") == 0) {
            opts->numeric = 1;
        } else if (strcmp(argv[i], "-C") == 0 || strcmp(argv[i], "--continuous") == 0) {
            opts->continuous = 1;
        } else if (strcmp(argv[i], "--interval") == 0) {
            if (i + 1 < argc) {
                opts->refresh_interval = atoi(argv[++i]);
                if (opts->refresh_interval < 1) opts->refresh_interval = 1;
            }
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
    
    // 如果没有指定任何显示选项，默认显示接口和连接
    if (!opts->show_interfaces && !opts->show_connections && 
        !opts->show_routing && !opts->show_arp && 
        !opts->show_dns && !opts->show_stats) {
        opts->show_interfaces = 1;
        opts->show_connections = 1;
    }
    
    return 1;
}

// 获取网络接口信息
static int get_interface_info(InterfaceInfo **interfaces) {
    struct ifaddrs *ifaddr, *ifa;
    *interfaces = NULL;
    int count = 0;
    
    if (getifaddrs(&ifaddr) == -1) {
        return 0;
    }
    
    // 第一次遍历计算接口数量
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            count++;
        }
    }
    
    if (count == 0) {
        freeifaddrs(ifaddr);
        return 0;
    }
    
    *interfaces = malloc(sizeof(InterfaceInfo) * count);
    memset(*interfaces, 0, sizeof(InterfaceInfo) * count);
    
    int index = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            InterfaceInfo *info = &(*interfaces)[index];
            
            // 接口名
            strncpy(info->name, ifa->ifa_name, sizeof(info->name) - 1);
            
            // 状态
            info->is_up = (ifa->ifa_flags & IFF_UP) != 0;
            info->is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;
            
            // IP地址
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, info->ip_addr, INET_ADDRSTRLEN);
            
            // 子网掩码
            if (ifa->ifa_netmask != NULL) {
                struct sockaddr_in *mask = (struct sockaddr_in *)ifa->ifa_netmask;
                inet_ntop(AF_INET, &mask->sin_addr, info->netmask, INET_ADDRSTRLEN);
            }
            
            // 广播地址
            if ((ifa->ifa_flags & IFF_BROADCAST) && ifa->ifa_broadaddr != NULL) {
                struct sockaddr_in *bcast = (struct sockaddr_in *)ifa->ifa_broadaddr;
                inet_ntop(AF_INET, &bcast->sin_addr, info->broadcast, INET_ADDRSTRLEN);
            }
            
            index++;
        }
    }
    
    freeifaddrs(ifaddr);
    
    // 获取MAC地址和流量统计
    for (int i = 0; i < count; i++) {
        char path[256];
        FILE *fp;
        
        // 获取MAC地址
        snprintf(path, sizeof(path), "/sys/class/net/%s/address", (*interfaces)[i].name);
        fp = fopen(path, "r");
        if (fp) {
            fgets((*interfaces)[i].mac_addr, sizeof((*interfaces)[i].mac_addr), fp);
            char *newline = strchr((*interfaces)[i].mac_addr, '\n');
            if (newline) *newline = '\0';
            fclose(fp);
        }
        
        // 获取接收字节数
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", (*interfaces)[i].name);
        fp = fopen(path, "r");
        if (fp) {
            fscanf(fp, "%lu", &(*interfaces)[i].rx_bytes);
            fclose(fp);
        }
        
        // 获取发送字节数
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", (*interfaces)[i].name);
        fp = fopen(path, "r");
        if (fp) {
            fscanf(fp, "%lu", &(*interfaces)[i].tx_bytes);
            fclose(fp);
        }
    }
    
    return count;
}

// 获取网络连接信息
static int get_connection_info(ConnectionInfo **connections, Options *opts) {
    FILE *fp;
    char line[512];
    *connections = NULL;
    int count = 0;
    int capacity = 16;
    
    *connections = malloc(sizeof(ConnectionInfo) * capacity);
    
    // 读取TCP连接
    if (!opts->udp_only) {
        fp = fopen("/proc/net/tcp", "r");
        if (fp) {
            fgets(line, sizeof(line), fp); // 跳过标题行
            
            while (fgets(line, sizeof(line), fp)) {
                if (count >= capacity) {
                    capacity *= 2;
                    *connections = realloc(*connections, sizeof(ConnectionInfo) * capacity);
                }
                
                ConnectionInfo *conn = &(*connections)[count];
                memset(conn, 0, sizeof(ConnectionInfo));
                strcpy(conn->protocol, "TCP");
                
                // 解析/proc/net/tcp格式
                unsigned int local_addr, foreign_addr;
                unsigned int local_port, foreign_port;
                char state[16];
                
                sscanf(line, "%*d: %x:%x %x:%x %s",
                       &local_addr, &local_port,
                       &foreign_addr, &foreign_port,
                       state);
                
                // 转换地址
                struct in_addr addr;
                addr.s_addr = htonl(local_addr);
                inet_ntop(AF_INET, &addr, conn->local_addr, sizeof(conn->local_addr));
                
                addr.s_addr = htonl(foreign_addr);
                inet_ntop(AF_INET, &addr, conn->foreign_addr, sizeof(conn->foreign_addr));
                
                conn->local_port = local_port;
                conn->foreign_port = foreign_port;
                
                // 转换状态
                int state_num = atoi(state);
                switch (state_num) {
                    case 1: strcpy(conn->state, "ESTABLISHED"); break;
                    case 2: strcpy(conn->state, "SYN_SENT"); break;
                    case 3: strcpy(conn->state, "SYN_RECV"); break;
                    case 4: strcpy(conn->state, "FIN_WAIT1"); break;
                    case 5: strcpy(conn->state, "FIN_WAIT2"); break;
                    case 6: strcpy(conn->state, "TIME_WAIT"); break;
                    case 7: strcpy(conn->state, "CLOSE"); break;
                    case 8: strcpy(conn->state, "CLOSE_WAIT"); break;
                    case 9: strcpy(conn->state, "LAST_ACK"); break;
                    case 10: strcpy(conn->state, "LISTEN"); break;
                    case 11: strcpy(conn->state, "CLOSING"); break;
                    default: strcpy(conn->state, "UNKNOWN"); break;
                }
                
                count++;
            }
            
            fclose(fp);
        }
    }
    
    // 读取UDP连接
    if (!opts->tcp_only) {
        fp = fopen("/proc/net/udp", "r");
        if (fp) {
            fgets(line, sizeof(line), fp); // 跳过标题行
            
            while (fgets(line, sizeof(line), fp)) {
                if (count >= capacity) {
                    capacity *= 2;
                    *connections = realloc(*connections, sizeof(ConnectionInfo) * capacity);
                }
                
                ConnectionInfo *conn = &(*connections)[count];
                memset(conn, 0, sizeof(ConnectionInfo));
                strcpy(conn->protocol, "UDP");
                
                // 解析/proc/net/udp格式
                unsigned int local_addr, foreign_addr;
                unsigned int local_port, foreign_port;
                
                sscanf(line, "%*d: %x:%x %x:%x",
                       &local_addr, &local_port,
                       &foreign_addr, &foreign_port);
                
                // 转换地址
                struct in_addr addr;
                addr.s_addr = htonl(local_addr);
                inet_ntop(AF_INET, &addr, conn->local_addr, sizeof(conn->local_addr));
                
                addr.s_addr = htonl(foreign_addr);
                inet_ntop(AF_INET, &addr, conn->foreign_addr, sizeof(conn->foreign_addr));
                
                conn->local_port = local_port;
                conn->foreign_port = foreign_port;
                strcpy(conn->state, "UNCONN");
                
                count++;
            }
            
            fclose(fp);
        }
    }
    
    return count;
}

// 获取路由表信息
static void show_routing_table(Options *opts) {
    FILE *fp = popen("ip route show 2>/dev/null || route -n 2>/dev/null", "r");
    if (!fp) return;
    
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🗺️  路由表:");
        printf("══════════════════════════════════════════════════════════════\n");
    } else {
        printf("路由表:\n");
        printf("══════════════════════════════════════════════════════════════\n");
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    
    pclose(fp);
    printf("\n");
}

// 获取ARP表信息
static void show_arp_table(Options *opts) {
    FILE *fp = popen("ip neigh show 2>/dev/null || arp -n 2>/dev/null", "r");
    if (!fp) return;
    
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "📡 ARP表:");
        printf("══════════════════════════════════════════════════════════════\n");
    } else {
        printf("ARP表:\n");
        printf("══════════════════════════════════════════════════════════════\n");
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    
    pclose(fp);
    printf("\n");
}

// 获取DNS信息
static void show_dns_info(Options *opts) {
    FILE *fp = fopen("/etc/resolv.conf", "r");
    if (!fp) return;
    
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🔍 DNS配置:");
        printf("══════════════════════════════════════════════════════════════\n");
    } else {
        printf("DNS配置:\n");
        printf("══════════════════════════════════════════════════════════════\n");
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "nameserver") || strstr(line, "search")) {
            printf("%s", line);
        }
    }
    
    fclose(fp);
    printf("\n");
}

// 获取网络统计信息
static void show_network_stats(Options *opts) {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return;
    
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "📊 网络统计:");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
    } else {
        printf("网络统计:\n");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
    }
    
    char line[256];
    fgets(line, sizeof(line), fp); // 跳过标题行
    fgets(line, sizeof(line), fp); // 跳过标题行
    
    while (fgets(line, sizeof(line), fp)) {
        char iface[32];
        unsigned long rx_bytes, rx_packets, rx_errors, rx_drop;
        unsigned long tx_bytes, tx_packets, tx_errors, tx_drop;
        
        sscanf(line, "%31s %lu %lu %lu %lu %*u %*u %*u %*u %lu %lu %lu %lu",
               iface, &rx_bytes, &rx_packets, &rx_errors, &rx_drop,
               &tx_bytes, &tx_packets, &tx_errors, &tx_drop);
        
        // 移除接口名后的冒号
        char *colon = strchr(iface, ':');
        if (colon) *colon = '\0';
        
        // 跳过回环接口
        if (strcmp(iface, "lo") == 0) continue;
        
        printf("%-10s RX: %10lu bytes %6lu packets  TX: %10lu bytes %6lu packets\n",
               iface, rx_bytes, rx_packets, tx_bytes, tx_packets);
    }
    
    fclose(fp);
    printf("\n");
}

// 显示网络接口信息
static void show_interfaces(InterfaceInfo *interfaces, int count, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "📡 网络接口:");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
    } else {
        printf("网络接口:\n");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
    }
    
    for (int i = 0; i < count; i++) {
        InterfaceInfo *info = &interfaces[i];
        
        // 跳过回环接口（除非指定显示所有）
        if (info->is_loopback && !opts->show_all) continue;
        
        if (opts->color) {
            // 接口状态颜色
            const char *status_color = info->is_up ? COLOR_BRIGHT_GREEN : COLOR_BRIGHT_RED;
            const char *status_text = info->is_up ? "UP" : "DOWN";
            
            // 接口名和状态
            color_print(status_color, "%-10s [%s]", info->name, status_text);
            
            if (info->is_loopback) {
                printf(" %s(loopback)%s", COLOR_BRIGHT_YELLOW, COLOR_RESET);
            }
            
            printf("\n");
            
            // MAC地址
            if (strlen(info->mac_addr) > 0) {
                color_print(COLOR_BRIGHT_BLUE, "  MAC地址:  ");
                printf("%s\n", info->mac_addr);
            }
            
            // IP地址
            if (strlen(info->ip_addr) > 0) {
                color_print(COLOR_BRIGHT_BLUE, "  IP地址:   ");
                printf("%s", info->ip_addr);
                
                if (strlen(info->netmask) > 0) {
                    printf(" / %s", info->netmask);
                }
                
                if (strlen(info->broadcast) > 0 && !info->is_loopback) {
                    printf(" (广播: %s)", info->broadcast);
                }
                
                printf("\n");
            }
            
            // 流量统计
            if (info->rx_bytes > 0 || info->tx_bytes > 0) {
                color_print(COLOR_BRIGHT_BLUE, "  流量统计: ");
                char *rx_str = format_size(info->rx_bytes);
                char *tx_str = format_size(info->tx_bytes);
                printf("RX: %s  TX: %s\n", rx_str, tx_str);
            }
            
        } else {
            // 无颜色输出
            printf("%-10s [%s]", info->name, info->is_up ? "UP" : "DOWN");
            if (info->is_loopback) printf(" (loopback)");
            printf("\n");
            
            if (strlen(info->mac_addr) > 0) {
                printf("  MAC地址:  %s\n", info->mac_addr);
            }
            
            if (strlen(info->ip_addr) > 0) {
                printf("  IP地址:   %s", info->ip_addr);
                if (strlen(info->netmask) > 0) {
                    printf(" / %s", info->netmask);
                }
                printf("\n");
            }
            
            if (info->rx_bytes > 0 || info->tx_bytes > 0) {
                char *rx_str = format_size(info->rx_bytes);
                char *tx_str = format_size(info->tx_bytes);
                printf("  流量统计: RX: %s  TX: %s\n", rx_str, tx_str);
            }
        }
        
        printf("\n");
    }
}

// 显示网络连接信息
static void show_connections(ConnectionInfo *connections, int count, Options *opts) {
    if (opts->color) {
        color_println(COLOR_BRIGHT_CYAN, "🔗 网络连接:");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
        color_print(COLOR_BRIGHT_YELLOW, "%-8s %-23s %-23s %-12s\n", 
                   "协议", "本地地址", "远程地址", "状态");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
    } else {
        printf("网络连接:\n");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
        printf("%-8s %-23s %-23s %-12s\n", 
               "协议", "本地地址", "远程地址", "状态");
        printf("══════════════════════════════════════════════════════════════════════════════\n");
    }
    
    int displayed = 0;
    for (int i = 0; i < count; i++) {
        ConnectionInfo *conn = &connections[i];
        
        // 过滤监听端口
        if (opts->listening_only) {
            if (strcmp(conn->state, "LISTEN") != 0 && 
                (strcmp(conn->protocol, "UDP") != 0 || strcmp(conn->state, "UNCONN") != 0)) {
                continue;
            }
        }
        
        // 构建地址字符串
        char local[64], foreign[64];
        if (opts->numeric) {
            snprintf(local, sizeof(local), "%s:%d", conn->local_addr, conn->local_port);
            snprintf(foreign, sizeof(foreign), "%s:%d", conn->foreign_addr, conn->foreign_port);
        } else {
            // 尝试解析主机名
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            
            inet_pton(AF_INET, conn->local_addr, &addr.sin_addr);
            snprintf(local, sizeof(local), "%s:%d", conn->local_addr, conn->local_port);
            
            inet_pton(AF_INET, conn->foreign_addr, &addr.sin_addr);
            snprintf(foreign, sizeof(foreign), "%s:%d", conn->foreign_addr, conn->foreign_port);
        }
        
        // 状态颜色
        const char *state_color = COLOR_RESET;
        if (opts->color) {
            if (strcmp(conn->state, "LISTEN") == 0) {
                state_color = COLOR_BRIGHT_YELLOW;
            } else if (strcmp(conn->state, "ESTABLISHED") == 0) {
                state_color = COLOR_BRIGHT_GREEN;
            } else if (strcmp(conn->state, "TIME_WAIT") == 0 || 
                      strcmp(conn->state, "CLOSE_WAIT") == 0) {
                state_color = COLOR_BRIGHT_MAGENTA;
            } else {
                state_color = COLOR_BRIGHT_RED;
            }
        }
        
        if (opts->color) {
            // 协议颜色
            if (strcmp(conn->protocol, "TCP") == 0) {
                printf("%s%-8s%s ", COLOR_BRIGHT_CYAN, conn->protocol, COLOR_RESET);
            } else {
                printf("%s%-8s%s ", COLOR_BRIGHT_BLUE, conn->protocol, COLOR_RESET);
            }
            
            printf("%-23s %-23s %s%-12s%s\n", 
                   local, foreign, state_color, conn->state, COLOR_RESET);
        } else {
            printf("%-8s %-23s %-23s %-12s\n", 
                   conn->protocol, local, foreign, conn->state);
        }
        
        displayed++;
    }
    
    if (displayed == 0) {
        printf("没有网络连接\n");
    }
    
    printf("\n");
}

// 显示分隔线
static void print_separator(const char *color) {
    if (color) {
        color_println(color, "══════════════════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
    }
}

// 显示标题
static void show_header(Options *opts, int iteration) {
    printf("\033[2J\033[H"); // 清屏
    
    if (opts->color) {
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
        color_print(COLOR_BRIGHT_MAGENTA, "                    tknet - 网络状态监控");
        if (opts->continuous) {
            printf(" (更新 #%d)", iteration);
        }
        printf("\n");
        color_println(COLOR_BRIGHT_MAGENTA, "══════════════════════════════════════════════════════════════");
    } else {
        printf("══════════════════════════════════════════════════════════════\n");
        printf("                    tknet - 网络状态监控");
        if (opts->continuous) {
            printf(" (更新 #%d)", iteration);
        }
        printf("\n");
        printf("══════════════════════════════════════════════════════════════\n");
    }
    
    // 显示当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    if (opts->color) {
        color_print(COLOR_BRIGHT_YELLOW, "时间: ");
        printf("%s", time_str);
        
        // 显示主机名
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            printf("  主机: %s", hostname);
        }
        
        printf("\n");
        print_separator(COLOR_BRIGHT_BLUE);
    } else {
        printf("时间: %s", time_str);
        
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            printf("  主机: %s", hostname);
        }
        
        printf("\n");
        print_separator(NULL);
    }
    
    printf("\n");
}

// 信号处理
static volatile int running = 1;

static void signal_handler(int sig) {
    running = 0;
}

// tknet主函数
int tknet_main(int argc, char **argv) {
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
    
    int iteration = 0;
    
    do {
        iteration++;
        
        // 显示标题
        if (opts.continuous || iteration == 1) {
            show_header(&opts, iteration);
        }
        
        // 显示网络接口信息
        if (opts.show_interfaces) {
            InterfaceInfo *interfaces;
            int iface_count = get_interface_info(&interfaces);
            if (iface_count > 0) {
                show_interfaces(interfaces, iface_count, &opts);
                free(interfaces);
            }
        }
        
        // 显示网络连接信息
        if (opts.show_connections) {
            ConnectionInfo *connections;
            int conn_count = get_connection_info(&connections, &opts);
            if (conn_count > 0) {
                show_connections(connections, conn_count, &opts);
                free(connections);
            }
        }
        
        // 显示路由表
        if (opts.show_routing) {
            show_routing_table(&opts);
        }
        
        // 显示ARP表
        if (opts.show_arp) {
            show_arp_table(&opts);
        }
        
        // 显示DNS信息
        if (opts.show_dns) {
            show_dns_info(&opts);
        }
        
        // 显示网络统计
        if (opts.show_stats) {
            show_network_stats(&opts);
        }
        
        // 显示退出提示（持续模式）
        if (opts.continuous) {
            printf("\n");
            if (opts.color) {
                color_println(COLOR_BRIGHT_YELLOW, "按 Ctrl+C 退出监控");
                color_println(COLOR_BRIGHT_CYAN, "══════════════════════════════════════════════════════════════");
            } else {
                printf("按 Ctrl+C 退出监控\n");
                printf("══════════════════════════════════════════════════════════════\n");
            }
            
            // 等待
            sleep(opts.refresh_interval);
        }
        
    } while (opts.continuous && running);
    
    return 0;
}