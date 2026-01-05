#include "ansi_unicode.h"
#include <stdio.h>
#include <string.h>

// ANSI颜色码定义（对应不同文件类型、git状态）
const char* ansi_color(FileType type, GitStatus status) {
    switch (type) {
        case FILE_REGULAR: return ANSI_RESET;          // 普通文件：默认色
        case FILE_DIR: return ANSI_COLOR_BLUE;         // 目录：蓝色
        case FILE_EXEC: return ANSI_COLOR_GREEN;       // 可执行文件：绿色
        case FILE_LINK: return ANSI_COLOR_CYAN;        // 软链接：青色
        case FILE_SPECIAL: return ANSI_COLOR_MAGENTA;  // 特殊文件：洋红
        default: return ANSI_RESET;
    }
    // git状态叠加颜色
    if (status == GIT_MODIFIED) return ANSI_COLOR_YELLOW;  // 已修改：黄色
    if (status == GIT_ADDED) return ANSI_COLOR_GREEN;      // 已添加：绿色
    if (status == GIT_DELETED) return ANSI_COLOR_RED;      // 已删除：红色
    return ANSI_RESET;
}

// 启用/关闭ANSI颜色输出
void ansi_enable(int enable) {
    if (enable) {
        setenv("TERM", "xterm-256color", 1);  // 声明支持256色终端
    } else {
        unsetenv("TERM");
    }
}

// Unicode图标渲染（文件类型对应图标，支持终端显示）
const char* unicode_icon(FileType type) {
    // 支持Unicode emoji/nerd font图标
    switch (type) {
        case FILE_DIR: return "📁";
        case FILE_EXEC: return "⚙️";
        case FILE_LINK: return "🔗";
        case FILE_IMAGE: return "🖼️";
        case FILE_VIDEO: return "🎬";
        case FILE_TEXT: return "📄";
        default: return "📎";
    }
}

// 安全打印Unicode字符串（避免乱码）
void print_unicode(const char* str) {
    #ifdef _WIN32
    // Windows下启用UTF-8控制台
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    #endif
    printf("%s", str);
}