#ifndef LS_ENHANCED_H
#define LS_ENHANCED_H

#include "../utils/ansi_unicode.h"
#include <stdint.h>
#include <string.h>

// 函数声明
FileType get_file_type(const char* path);
GitStatus get_git_status(const char* file_path);
void ls_enhanced(const char* dir_path);
const char* git_status_str(GitStatus status);

#endif // LS_ENHANCED_H