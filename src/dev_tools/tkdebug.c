// src/dev_tools/tkdebug.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include "../common/colors.h"
#include "../common/utils.h"
#include "../common/progress.h"

#define MAX_BUFFER 4096
#define DEFAULT_TIMEOUT 5

typedef struct {
    char host[256];
    int port;
    int timeout;
    int verbose;
} DebugConfig;

void print_help() {
    printf("tkdebug - 网络调试工具\n\n");
    printf("用法:\n");
    printf("  tkdebug [命令] [选项] [参数]\n\n");
    printf("命令:\n");
    printf("  ping <主机>           Ping测试\n");
    printf("  scan <主机> [端口]    端口扫描\n");
    printf("  http <URL>           HTTP请求测试\n");
    printf("  dns <域名>           DNS查询\n\n");
    
    printf("选项:\n");
    printf("  -p <端口>            指定端口\n");
    printf("  -t <秒>              超时时间(默认5秒)\n");
    printf("  -v                   详细输出\n");
    printf("  -h                   显示帮助\n\n");
    
    printf("示例:\n");
    printf("  tkdebug ping google.com\n");
    printf("  tkdebug scan example.com 80 443\n");
    printf("  tkdebug http http://example.com\n");
    printf("  tkdebug dns google.com -v\n");
}

// 简单的Ping功能（使用TCP连接模拟）
int ping_host(const char *host, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        print_error("创建socket失败: %s", strerror(errno));
        return 0;
    }
    
    // 设置超时
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    // 解析主机名
    struct hostent *server = gethostbyname(host);
    if (!server) {
        print_error("无法解析主机名: %s", host);
        close(sock);
        return 0;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    color_print(COLOR_CYAN, "PING %s:%d ", host, port);
    fflush(stdout);
    
    clock_t start = clock();
    int result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    clock_t end = clock();
    
    if (result == 0) {
        double time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000;
        color_println(COLOR_GREEN, "成功 - %.2f ms", time_ms);
        close(sock);
        return 1;
    } else {
        color_println(COLOR_RED, "失败 - %s", strerror(errno));
        close(sock);
        return 0;
    }
}

// 端口扫描
void scan_ports(const char *host, int start_port, int end_port, int timeout, int verbose) {
    printf("扫描 %s 端口 %d-%d\n", host, start_port, end_port);
    
    int total_ports = end_port - start_port + 1;
    int open_ports = 0;
    
    ProgressBar *bar = progress_create(50, COLOR_BRIGHT_BLUE);
    
    for (int port = start_port; port <= end_port; port++) {
        float progress = (float)(port - start_port) / total_ports;
        progress_show(bar, progress, "扫描中...");
        
        if (ping_host(host, port, timeout)) {
            open_ports++;
            if (verbose) {
                // 尝试获取服务名称
                struct servent *service = getservbyport(htons(port), "tcp");
                if (service) {
                    printf("\n端口 %d 开放 - %s", port, service->s_name);
                } else {
                    printf("\n端口 %d 开放", port);
                }
            }
        }
    }
    
    progress_finish(bar, "扫描完成");
    progress_destroy(bar);
    
    printf("\n扫描结果: %s\n", host);
    printf("扫描端口范围: %d-%d\n", start_port, end_port);
    printf("开放端口: %d/%d\n", open_ports, total_ports);
    if (open_ports == 0) {
        color_println(COLOR_YELLOW, "未发现开放端口");
    }
}

// HTTP请求测试
void http_test(const char *url) {
    printf("HTTP测试: %s\n", url);
    
    // 解析URL
    char host[256] = {0};
    char path[256] = "/";
    int port = 80;
    
    // 简单的URL解析
    if (strstr(url, "http://") == url) {
        const char *host_start = url + 7;
        const char *path_start = strchr(host_start, '/');
        
        if (path_start) {
            int host_len = path_start - host_start;
            strncpy(host, host_start, host_len);
            strcpy(path, path_start);
        } else {
            strcpy(host, host_start);
        }
        
        // 检查端口
        char *colon = strchr(host, ':');
        if (colon) {
            *colon = '\0';
            port = atoi(colon + 1);
        }
    } else {
        strcpy(host, url);
    }
    
    printf("主机: %s\n", host);
    printf("端口: %d\n", port);
    printf("路径: %s\n", path);
    
    // 创建socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        print_error("创建socket失败");
        return;
    }
    
    // 解析主机
    struct hostent *server = gethostbyname(host);
    if (!server) {
        print_error("无法解析主机名");
        close(sock);
        return;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    // 连接
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        print_error("连接失败: %s", strerror(errno));
        close(sock);
        return;
    }
    
    // 发送HTTP请求
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: tkdebug/1.0\r\n"
             "Connection: close\r\n\r\n",
             path, host);
    
    if (send(sock, request, strlen(request), 0) < 0) {
        print_error("发送请求失败");
        close(sock);
        return;
    }
    
    // 接收响应
    printf("\n响应:\n");
    printf("====================\n");
    
    char buffer[MAX_BUFFER];
    int total_bytes = 0;
    int headers_done = 0;
    
    while (1) {
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        
        buffer[bytes] = '\0';
        total_bytes += bytes;
        
        // 分离头部和正文
        if (!headers_done) {
            char *body_start = strstr(buffer, "\r\n\r\n");
            if (body_start) {
                headers_done = 1;
                *body_start = '\0';
                printf("%s\n", buffer);  // 打印头部
                printf("====================\n");
                printf("%s", body_start + 4);  // 打印正文
            } else {
                printf("%s", buffer);
            }
        } else {
            printf("%s", buffer);
        }
    }
    
    printf("\n====================\n");
    printf("总接收字节: %d\n", total_bytes);
    
    close(sock);
}

// DNS查询
void dns_query(const char *domain, int verbose) {
    printf("DNS查询: %s\n", domain);
    
    struct hostent *host = gethostbyname(domain);
    if (!host) {
        print_error("DNS查询失败: %s", hstrerror(h_errno));
        return;
    }
    
    printf("正式主机名: %s\n", host->h_name);
    
    // 别名
    if (verbose && host->h_aliases && host->h_aliases[0]) {
        printf("别名:\n");
        for (int i = 0; host->h_aliases[i]; i++) {
            printf("  %s\n", host->h_aliases[i]);
        }
    }
    
    // IP地址
    printf("IP地址:\n");
    for (int i = 0; host->h_addr_list[i]; i++) {
        struct in_addr addr;
        memcpy(&addr, host->h_addr_list[i], sizeof(addr));
        printf("  %s\n", inet_ntoa(addr));
    }
    
    printf("地址类型: %s\n", 
           host->h_addrtype == AF_INET ? "IPv4" : 
           host->h_addrtype == AF_INET6 ? "IPv6" : "未知");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }
    
    char *command = argv[1];
    DebugConfig config = {
        .timeout = DEFAULT_TIMEOUT,
        .verbose = 0,
        .port = 0
    };
    
    // 解析全局选项
    int opt;
    while ((opt = getopt(argc, argv, "p:t:vh")) != -1) {
        switch (opt) {
            case 'p':
                config.port = atoi(optarg);
                break;
            case 't':
                config.timeout = atoi(optarg);
                if (config.timeout <= 0) config.timeout = DEFAULT_TIMEOUT;
                break;
            case 'v':
                config.verbose = 1;
                break;
            case 'h':
                print_help();
                return 0;
            default:
                break;
        }
    }
    
    // 根据命令执行相应功能
    if (strcmp(command, "ping") == 0) {
        if (optind >= argc) {
            print_error("需要指定主机名");
            return 1;
        }
        char *host = argv[optind];
        int port = config.port ? config.port : 80;
        
        printf("Ping测试 %s (端口 %d)\n", host, port);
        printf("================================\n");
        
        int success = 0;
        for (int i = 0; i < 4; i++) {  // 发送4个ping
            success += ping_host(host, port, config.timeout);
            if (i < 3) sleep(1);  // 间隔1秒
        }
        
        printf("\n统计: 成功 %d/4\n", success);
        
    } else if (strcmp(command, "scan") == 0) {
        if (optind >= argc) {
            print_error("需要指定主机名");
            return 1;
        }
        char *host = argv[optind];
        
        int start_port = 1;
        int end_port = 100;  // 默认扫描前100个端口
        
        if (optind + 1 < argc) {
            start_port = atoi(argv[optind + 1]);
            end_port = atoi(argv[optind + 2]);
        }
        
        if (start_port <= 0 || end_port <= 0 || start_port > end_port) {
            print_error("无效的端口范围");
            return 1;
        }
        
        if (end_port - start_port > 1000) {
            print_warning("扫描范围较大，可能需要较长时间");
        }
        
        scan_ports(host, start_port, end_port, config.timeout, config.verbose);
        
    } else if (strcmp(command, "http") == 0) {
        if (optind >= argc) {
            print_error("需要指定URL");
            return 1;
        }
        char *url = argv[optind];
        http_test(url);
        
    } else if (strcmp(command, "dns") == 0) {
        if (optind >= argc) {
            print_error("需要指定域名");
            return 1;
        }
        char *domain = argv[optind];
        dns_query(domain, config.verbose);
        
    } else {
        print_error("未知命令: %s", command);
        print_help();
        return 1;
    }
    
    return 0;
}