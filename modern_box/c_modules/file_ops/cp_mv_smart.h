#ifndef CP_MV_SMART_H
#define CP_MV_SMART_H

#include <stdint.h>
#include <sys/types.h>

// 函数声明
off_t get_file_size(const char* file_path);
int smart_cp(const char* src_path, const char* dst_path);
int smart_mv(const char* src_path, const char* dst_path);

#endif // CP_MV_SMART_H