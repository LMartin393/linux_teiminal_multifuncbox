// src/common/utils.h
#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

// 错误和信息输出
void print_error(const char *format, ...);
void print_warning(const char *format, ...);
void print_info(const char *format, ...);
void print_success(const char *format, ...);

// 文件检查
int is_directory(const char *path);
int file_exists(const char *path);
int is_file(const char *path);
int is_readable(const char *path);

// 格式化
char* format_size(long long bytes);
char* format_time(time_t timestamp);

// 字符串处理（仅最基本）
void trim_string(char *str);
int starts_with(const char *str, const char *prefix);

#endif // UTILS_H