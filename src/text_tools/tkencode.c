// src/text_tools/tkencode.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <iconv.h>
#include <errno.h>
#include <locale.h>
#include "../common/colors.h"
#include "../common/utils.h"

#define BUFFER_SIZE 4096
#define MAX_ENCODING 32

typedef struct {
    char input_enc[MAX_ENCODING];
    char output_enc[MAX_ENCODING];
    int verbose;
    int list_encodings;
    int detect_encoding;
    int show_bom;
    int force;
    char input_file[256];
    char output_file[256];
} Config;

void print_help() {
    printf("tkencode - 文件编码转换工具\n\n");
    printf("用法:\n");
    printf("  tkencode [选项] [输入文件] [输出文件]\n\n");
    
    printf("选项:\n");
    printf("  -f <编码>    输入文件编码（默认：自动检测）\n");
    printf("  -t <编码>    输出文件编码（默认：UTF-8）\n");
    printf("  -l           列出支持的编码\n");
    printf("  -d           检测文件编码\n");
    printf("  -b           显示/添加BOM（字节顺序标记）\n");
    printf("  -F           强制转换（忽略错误）\n");
    printf("  -v           详细输出\n");
    printf("  -h           显示帮助\n\n");
    
    printf("常用编码:\n");
    printf("  UTF-8, UTF-16LE, UTF-16BE, UTF-32LE, UTF-32BE\n");
    printf("  GBK, GB2312, GB18030, BIG5, EUC-JP, SHIFT-JIS\n");
    printf("  ISO-8859-1, ISO-8859-15, ASCII, CP936, CP950\n\n");
    
    printf("示例:\n");
    printf("  tkencode -f GBK -t UTF-8 input.txt output.txt\n");
    printf("  tkencode -d file.txt                 # 检测编码\n");
    printf("  tkencode -l                          # 列出编码\n");
    printf("  cat input.txt | tkencode -f GBK      # 从标准输入转换\n");
}

// 简单检测文件编码（通过BOM）
const char* detect_bom_encoding(FILE *file) {
    unsigned char bom[4];
    long pos = ftell(file);
    
    if (fread(bom, 1, 4, file) >= 2) {
        // UTF-16LE BOM: FF FE
        if (bom[0] == 0xFF && bom[1] == 0xFE) {
            if (bom[2] == 0x00 && bom[3] == 0x00) {
                return "UTF-32LE";
            }
            return "UTF-16LE";
        }
        // UTF-16BE BOM: FE FF
        else if (bom[0] == 0xFE && bom[1] == 0xFF) {
            if (bom[2] == 0x00 && bom[3] == 0x00) {
                return "UTF-32BE";
            }
            return "UTF-16BE";
        }
        // UTF-8 BOM: EF BB BF
        else if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
            return "UTF-8";
        }
    }
    
    fseek(file, pos, SEEK_SET);
    return NULL;
}

// 简单的编码猜测（基于字符范围）
const char* guess_encoding(const char *buffer, size_t length) {
    int utf8_score = 0;
    int ascii_score = 0;
    int gbk_score = 0;
    
    for (size_t i = 0; i < length; i++) {
        unsigned char c = buffer[i];
        
        if (c <= 0x7F) {
            ascii_score++;
            if (i + 1 < length) {
                // 检查UTF-8多字节序列
                if ((c & 0xE0) == 0xC0) {  // 2字节
                    if (i + 1 < length && (buffer[i+1] & 0xC0) == 0x80) {
                        utf8_score += 2;
                        i++;
                    }
                } else if ((c & 0xF0) == 0xE0) {  // 3字节
                    if (i + 2 < length && (buffer[i+1] & 0xC0) == 0x80 && 
                        (buffer[i+2] & 0xC0) == 0x80) {
                        utf8_score += 3;
                        i += 2;
                    }
                }
            }
        } else if (c >= 0x81 && c <= 0xFE) {
            // 可能是GBK/GB2312
            if (i + 1 < length) {
                unsigned char next = buffer[i+1];
                if ((next >= 0x40 && next <= 0x7E) || (next >= 0x80 && next <= 0xFE)) {
                    gbk_score += 2;
                    i++;
                }
            }
        }
    }
    
    if (utf8_score > gbk_score && utf8_score > ascii_score / 2) {
        return "UTF-8";
    } else if (gbk_score > utf8_score * 2) {
        return "GBK";
    }
    
    return "ASCII";
}

// 列出支持的编码
void list_supported_encodings() {
    printf("支持的编码列表:\n\n");
    
    color_println(COLOR_CYAN, "Unicode 系列:");
    printf("  UTF-8, UTF-16, UTF-16LE, UTF-16BE\n");
    printf("  UTF-32, UTF-32LE, UTF-32BE\n");
    printf("  UCS-2, UCS-4\n\n");
    
    color_println(COLOR_CYAN, "中文编码:");
    printf("  GBK, GB2312, GB18030\n");
    printf("  BIG5, BIG5-HKSCS\n");
    printf("  CP936 (简体中文Windows)\n");
    printf("  CP950 (繁体中文Windows)\n\n");
    
    color_println(COLOR_CYAN, "日文编码:");
    printf("  EUC-JP, SHIFT-JIS, ISO-2022-JP\n");
    printf("  CP932 (日文Windows)\n\n");
    
    color_println(COLOR_CYAN, "韩文编码:");
    printf("  EUC-KR, CP949\n\n");
    
    color_println(COLOR_CYAN, "西欧编码:");
    printf("  ISO-8859-1 (Latin-1)\n");
    printf("  ISO-8859-2 (Latin-2)\n");
    printf("  ISO-8859-15 (Latin-9)\n");
    printf("  CP1252 (西欧Windows)\n");
    printf("  ASCII, US-ASCII\n\n");
    
    color_println(COLOR_CYAN, "其他编码:");
    printf("  KOI8-R (俄文)\n");
    printf("  ISO-8859-5 (西里尔文)\n");
    printf("  CP1251 (西里尔文Windows)\n");
    
    printf("\n注意: 实际支持的编码取决于系统的iconv库\n");
}

// 添加BOM到输出
int add_bom(FILE *file, const char *encoding) {
    if (strcmp(encoding, "UTF-8") == 0) {
        unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        return fwrite(bom, 1, 3, file) == 3;
    } else if (strcmp(encoding, "UTF-16LE") == 0) {
        unsigned char bom[] = {0xFF, 0xFE};
        return fwrite(bom, 1, 2, file) == 2;
    } else if (strcmp(encoding, "UTF-16BE") == 0) {
        unsigned char bom[] = {0xFE, 0xFF};
        return fwrite(bom, 1, 2, file) == 2;
    } else if (strcmp(encoding, "UTF-32LE") == 0) {
        unsigned char bom[] = {0xFF, 0xFE, 0x00, 0x00};
        return fwrite(bom, 1, 4, file) == 4;
    } else if (strcmp(encoding, "UTF-32BE") == 0) {
        unsigned char bom[] = {0x00, 0x00, 0xFE, 0xFF};
        return fwrite(bom, 1, 4, file) == 4;
    }
    return 1;  // 不需要BOM
}

// 检测文件编码
void detect_file_encoding(const char *filename, Config *config) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        print_error("无法打开文件: %s", filename);
        return;
    }
    
    // 检查BOM
    const char *bom_enc = detect_bom_encoding(file);
    if (bom_enc) {
        color_println(COLOR_GREEN, "检测到BOM: %s", bom_enc);
    }
    
    // 读取一部分内容进行分析
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    
    if (bytes_read > 0) {
        const char *guessed = guess_encoding((char*)buffer, bytes_read);
        
        printf("文件分析结果:\n");
        printf("  文件大小: %s\n", format_size(bytes_read));
        printf("  采样大小: %zu 字节\n", bytes_read);
        
        if (bom_enc) {
            printf("  BOM编码: %s\n", bom_enc);
        }
        
        printf("  猜测编码: %s\n", guessed);
        
        // 显示字符统计
        int ascii_count = 0;
        int high_byte_count = 0;
        
        for (size_t i = 0; i < bytes_read; i++) {
            if (buffer[i] <= 0x7F) {
                ascii_count++;
            } else if (buffer[i] >= 0x80) {
                high_byte_count++;
            }
        }
        
        printf("  ASCII字符: %d (%.1f%%)\n", 
               ascii_count, (float)ascii_count * 100 / bytes_read);
        printf("  高位字符: %d (%.1f%%)\n", 
               high_byte_count, (float)high_byte_count * 100 / bytes_read);
    }
    
    fclose(file);
}

// 转换文件编码
int convert_encoding(Config *config) {
    iconv_t cd;
    FILE *in_file, *out_file;
    int use_stdin = (strlen(config->input_file) == 0);
    int use_stdout = (strlen(config->output_file) == 0);
    
    // 打开输入文件
    if (use_stdin) {
        in_file = stdin;
        if (config->verbose) {
            printf("从标准输入读取...\n");
        }
    } else {
        in_file = fopen(config->input_file, "rb");
        if (!in_file) {
            print_error("无法打开输入文件: %s", config->input_file);
            return 0;
        }
    }
    
    // 打开输出文件
    if (use_stdout) {
        out_file = stdout;
        setlocale(LC_ALL, "");  // 设置本地化，以便正确处理宽字符
        if (config->verbose) {
            printf("输出到标准输出...\n");
        }
    } else {
        out_file = fopen(config->output_file, "wb");
        if (!out_file) {
            print_error("无法创建输出文件: %s", config->output_file);
            if (!use_stdin) fclose(in_file);
            return 0;
        }
    }
    
    // 自动检测输入编码
    char detected_enc[MAX_ENCODING] = {0};
    if (strlen(config->input_enc) == 0 && !use_stdin) {
        const char *bom_enc = detect_bom_encoding(in_file);
        if (bom_enc) {
            strcpy(detected_enc, bom_enc);
            if (config->verbose) {
                printf("检测到输入编码(BOM): %s\n", detected_enc);
            }
        } else {
            // 读取部分内容猜测
            long pos = ftell(in_file);
            unsigned char buffer[BUFFER_SIZE];
            size_t bytes_read = fread(buffer, 1, sizeof(buffer), in_file);
            fseek(in_file, pos, SEEK_SET);
            
            if (bytes_read > 0) {
                const char *guessed = guess_encoding((char*)buffer, bytes_read);
                strcpy(detected_enc, guessed);
                if (config->verbose) {
                    printf("猜测输入编码: %s\n", detected_enc);
                }
            }
        }
    }
    
    // 准备iconv转换描述符
    const char *from_enc = (strlen(config->input_enc) > 0) ? 
                          config->input_enc : 
                          (strlen(detected_enc) > 0) ? detected_enc : "UTF-8";
    
    const char *to_enc = (strlen(config->output_enc) > 0) ? 
                        config->output_enc : "UTF-8";
    
    cd = iconv_open(to_enc, from_enc);
    if (cd == (iconv_t)-1) {
        print_error("不支持的编码转换: %s -> %s", from_enc, to_enc);
        if (!use_stdin) fclose(in_file);
        if (!use_stdout) fclose(out_file);
        return 0;
    }
    
    if (config->verbose) {
        printf("转换: %s -> %s\n", from_enc, to_enc);
    }
    
    // 添加BOM（如果需要）
    if (config->show_bom && !use_stdout) {
        if (!add_bom(out_file, to_enc)) {
            print_error("无法添加BOM");
        } else if (config->verbose) {
            printf("已添加BOM\n");
        }
    }
    
    // 执行转换
    char in_buf[BUFFER_SIZE];
    char out_buf[BUFFER_SIZE * 4];  // 输出缓冲区更大
    size_t total_in = 0, total_out = 0;
    int errors = 0;
    
    while (!feof(in_file)) {
        size_t in_size = fread(in_buf, 1, sizeof(in_buf), in_file);
        if (in_size == 0 && feof(in_file)) break;
        
        char *in_ptr = in_buf;
        size_t in_remaining = in_size;
        total_in += in_size;
        
        while (in_remaining > 0) {
            char *out_ptr = out_buf;
            size_t out_remaining = sizeof(out_buf);
            
            size_t result = iconv(cd, &in_ptr, &in_remaining, &out_ptr, &out_remaining);
            
            // 写入转换后的数据
            size_t converted = sizeof(out_buf) - out_remaining;
            if (converted > 0) {
                fwrite(out_buf, 1, converted, out_file);
                total_out += converted;
            }
            
            if (result == (size_t)-1) {
                if (errno == EILSEQ || errno == EINVAL) {
                    errors++;
                    if (config->force) {
                        // 跳过无效字符
                        in_ptr++;
                        in_remaining--;
                    } else {
                        print_error("转换错误: 无效字符 (位置: %zu)", total_in - in_remaining);
                        iconv_close(cd);
                        if (!use_stdin) fclose(in_file);
                        if (!use_stdout) fclose(out_file);
                        return 0;
                    }
                } else if (errno == E2BIG) {
                    // 输出缓冲区不足，继续循环
                    continue;
                }
            }
        }
    }
    
    // 刷新iconv缓冲区
    char *out_ptr = out_buf;
    size_t out_remaining = sizeof(out_buf);
    iconv(cd, NULL, NULL, &out_ptr, &out_remaining);
    size_t flushed = sizeof(out_buf) - out_remaining;
    if (flushed > 0) {
        fwrite(out_buf, 1, flushed, out_file);
        total_out += flushed;
    }
    
    iconv_close(cd);
    
    if (!use_stdin) fclose(in_file);
    if (!use_stdout) fclose(out_file);
    
    if (config->verbose) {
        printf("转换完成:\n");
        printf("  输入大小: %s\n", format_size(total_in));
        printf("  输出大小: %s\n", format_size(total_out));
        printf("  转换错误: %d\n", errors);
        if (!use_stdout) {
            printf("  输出文件: %s\n", config->output_file);
        }
    }
    
    return 1;
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");  // 设置本地化
    
    Config config = {
        .input_enc = "",
        .output_enc = "UTF-8",
        .verbose = 0,
        .list_encodings = 0,
        .detect_encoding = 0,
        .show_bom = 0,
        .force = 0,
        .input_file = "",
        .output_file = ""
    };
    
    // 解析选项
    int opt;
    while ((opt = getopt(argc, argv, "f:t:ldbFvh")) != -1) {
        switch (opt) {
            case 'f':
                strncpy(config.input_enc, optarg, sizeof(config.input_enc) - 1);
                break;
            case 't':
                strncpy(config.output_enc, optarg, sizeof(config.output_enc) - 1);
                break;
            case 'l':
                config.list_encodings = 1;
                break;
            case 'd':
                config.detect_encoding = 1;
                break;
            case 'b':
                config.show_bom = 1;
                break;
            case 'F':
                config.force = 1;
                break;
            case 'v':
                config.verbose = 1;
                break;
            case 'h':
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }
    
    // 列出编码
    if (config.list_encodings) {
        list_supported_encodings();
        return 0;
    }
    
    // 获取文件参数
    if (optind < argc) {
        strncpy(config.input_file, argv[optind], sizeof(config.input_file) - 1);
        optind++;
    }
    if (optind < argc) {
        strncpy(config.output_file, argv[optind], sizeof(config.output_file) - 1);
    }
    
    // 检测编码模式
    if (config.detect_encoding) {
        if (strlen(config.input_file) == 0) {
            print_error("需要指定要检测的文件");
            return 1;
        }
        detect_file_encoding(config.input_file, &config);
        return 0;
    }
    
    // 执行转换
    if (!convert_encoding(&config)) {
        return 1;
    }
    
    return 0;
}