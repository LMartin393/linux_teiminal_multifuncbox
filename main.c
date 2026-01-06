// main.c - 完整可编译版本
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// 所有工具函数声明
int tkls_main(int argc, char **argv);
int tkcpmv_main(int argc, char **argv);
int tkfind_main(int argc, char **argv);
int tkdiff_main(int argc, char **argv);
int tkgrep_main(int argc, char **argv);
int tkstream_main(int argc, char **argv);
int tkview_main(int argc, char **argv);
int tkencode_main(int argc, char **argv);
int tkinfo_main(int argc, char **argv);
int tkmon_main(int argc, char **argv);
int tknet_main(int argc, char **argv);
int tkhw_main(int argc, char **argv);
int tkcode_main(int argc, char **argv);
int tkconvert_main(int argc, char **argv);
int tkregex_main(int argc, char **argv);
int tkdebug_main(int argc, char **argv);

// 工具信息结构体
typedef struct {
    const char *name;
    const char *category;
    const char *desc;
    int (*main_func)(int, char**);
} ToolInfo;

// 工具注册表
ToolInfo tools[] = {
    {"tkls",    "file", "增强版ls（彩色+图标+git状态）", tkls_main},
    {"tkcpmv",  "file", "智能cp/mv（进度条+断点续传）", tkcpmv_main},
    {"tkfind",  "file", "增强版find（正则+内容搜索）", tkfind_main},
    {"tkdiff",  "file", "文件比较和合并", tkdiff_main},
    {"tkgrep",  "text", "增强版grep（高亮+上下文）", tkgrep_main},
    {"tkstream","text", "流式文本处理", tkstream_main},
    {"tkview",  "text", "CSV/JSON文件查看", tkview_main},
    {"tkencode","text", "编码转换", tkencode_main},
    {"tkinfo",  "sys",  "美观的系统信息显示", tkinfo_main},
    {"tkmon",   "sys",  "实时系统监控", tkmon_main},
    {"tknet",   "sys",  "网络状态查看", tknet_main},
    {"tkhw",    "sys",  "硬件信息检测", tkhw_main},
    {"tkcode",  "dev",  "代码统计工具", tkcode_main},
    {"tkconvert","dev", "文件格式转换", tkconvert_main},
    {"tkregex", "dev",  "正则表达式测试", tkregex_main},
    {"tkdebug", "dev",  "网络调试工具", tkdebug_main},
    {"exit",    "builtin", "退出", NULL},
    {"quit",    "builtin", "退出", NULL},
    {"help",    "builtin", "帮助", NULL},
    {"clear",   "builtin", "清屏", NULL},
    {NULL, NULL, NULL, NULL}
};

// 简单交互模式
void interactive_mode() {
    printf("TermKit交互模式 (输入help查看帮助)\n");
    
    char line[256];
    while (1) {
        printf("termkit> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        
        if (strlen(line) == 0) continue;
        
        // 内置命令
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        if (strcmp(line, "clear") == 0) { system("clear"); continue; }
        if (strcmp(line, "help") == 0) {
            printf("可用命令:\n");
            for (int i = 0; tools[i].name; i++) {
                printf("  %-10s %s\n", tools[i].name, tools[i].desc);
            }
            continue;
        }
        
        // 查找工具
        int found = 0;
        for (int i = 0; tools[i].name; i++) {
            if (strcmp(tools[i].name, line) == 0 && tools[i].main_func) {
                char *args[] = {tools[i].name};
                tools[i].main_func(1, args);
                found = 1;
                break;
            }
        }
        
        if (!found) printf("未知命令: %s\n", line);
    }
}

int main(int argc, char **argv) {
    if (argc == 1) {
        interactive_mode();
        return 0;
    }
    
    char *cmd = argv[1];
    
    // 全局选项
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        printf("TermKit - 终端多功能工具箱\n");
        printf("用法: termkit <工具名> [参数]\n\n");
        for (int i = 0; tools[i].name; i++) {
            if (tools[i].main_func) {
                printf("  %-10s %s\n", tools[i].name, tools[i].desc);
            }
        }
        return 0;
    }
    
    if (strcmp(cmd, "--list") == 0) {
        for (int i = 0; tools[i].name; i++) {
            if (tools[i].main_func) printf("%s\n", tools[i].name);
        }
        return 0;
    }
    
    // 查找并执行工具
    for (int i = 0; tools[i].name; i++) {
        if (strcmp(tools[i].name, cmd) == 0 && tools[i].main_func) {
            return tools[i].main_func(argc-1, argv+1);
        }
    }
    
    printf("未知工具: %s\n", cmd);
    return 1;
}