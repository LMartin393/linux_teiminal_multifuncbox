// src/dev_tools/tkconvert.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../common/colors.h"
#include "../common/utils.h"

void print_help() {
    printf("tkconvert - 文件格式转换工具\n\n");
    printf("用法:\n");
    printf("  tkconvert [选项] <输入文件> <输出文件>\n\n");
    printf("选项:\n");
    printf("  -t <类型>  指定转换类型 (text, html, json, xml)\n");
    printf("  -e <编码>  指定编码转换 (gbk, utf8, utf16, ascii)\n");
    printf("  -l         列出支持的格式\n");
    printf("  -h         显示帮助\n\n");
    printf("示例:\n");
    printf("  tkconvert -t json data.txt data.json\n");
    printf("  tkconvert -e gbktoutf8 input.txt output.txt\n");
}

void list_formats() {
    color_println(COLOR_CYAN, "支持的转换类型:");
    printf("  text -> html   文本转HTML\n");
    printf("  text -> json   文本转JSON\n");
    printf("  text -> xml    文本转XML\n");
    printf("  json -> text   JSON转文本\n");
    printf("  xml -> text    XML转文本\n\n");
    
    color_println(COLOR_CYAN, "支持的编码:");
    printf("  utf8 -> gbk    UTF-8转GBK\n");
    printf("  gbk -> utf8    GBK转UTF-8\n");
    printf("  utf8 -> utf16  UTF-8转UTF-16\n");
    printf("  ascii -> utf8  ASCII转UTF-8\n");
}

// 简单的文本转HTML
int text_to_html(const char *input_file, const char *output_file) {
    FILE *in = fopen(input_file, "r");
    if (!in) {
        print_error("无法打开输入文件: %s", input_file);
        return 0;
    }
    
    FILE *out = fopen(output_file, "w");
    if (!out) {
        print_error("无法创建输出文件: %s", output_file);
        fclose(in);
        return 0;
    }
    
    fprintf(out, "<!DOCTYPE html>\n");
    fprintf(out, "<html>\n<head>\n");
    fprintf(out, "  <meta charset=\"utf-8\">\n");
    fprintf(out, "  <title>%s</title>\n", input_file);
    fprintf(out, "  <style>\n");
    fprintf(out, "    body { font-family: monospace; margin: 20px; }\n");
    fprintf(out, "    pre { background: #f5f5f5; padding: 10px; }\n");
    fprintf(out, "  </style>\n");
    fprintf(out, "</head>\n<body>\n");
    fprintf(out, "<h1>%s</h1>\n<pre>\n", input_file);
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), in)) {
        // 简单的HTML转义
        for (char *p = buffer; *p; p++) {
            switch (*p) {
                case '<': fprintf(out, "&lt;"); break;
                case '>': fprintf(out, "&gt;"); break;
                case '&': fprintf(out, "&amp;"); break;
                default: fputc(*p, out);
            }
        }
    }
    
    fprintf(out, "</pre>\n</body>\n</html>\n");
    
    fclose(in);
    fclose(out);
    return 1;
}

// 简单的文本转JSON
int text_to_json(const char *input_file, const char *output_file) {
    FILE *in = fopen(input_file, "r");
    if (!in) return 0;
    
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fclose(in);
        return 0;
    }
    
    fprintf(out, "{\n");
    fprintf(out, "  \"filename\": \"%s\",\n", input_file);
    fprintf(out, "  \"content\": [\n");
    
    char buffer[1024];
    int line_num = 0;
    while (fgets(buffer, sizeof(buffer), in)) {
        trim_string(buffer);
        if (line_num > 0) fprintf(out, ",\n");
        fprintf(out, "    {\n");
        fprintf(out, "      \"line\": %d,\n", line_num + 1);
        fprintf(out, "      \"text\": \"%s\"\n", buffer);
        fprintf(out, "    }");
        line_num++;
    }
    
    fprintf(out, "\n  ]\n}\n");
    
    fclose(in);
    fclose(out);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    char *conversion_type = NULL;
    char *encoding = NULL;
    char *input_file = NULL;
    char *output_file = NULL;
    
    // 解析参数
    int opt;
    while ((opt = getopt(argc, argv, "t:e:lh")) != -1) {
        switch (opt) {
            case 't':
                conversion_type = optarg;
                break;
            case 'e':
                encoding = optarg;
                break;
            case 'l':
                list_formats();
                return 0;
            case 'h':
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }
    
    // 获取输入输出文件
    if (optind < argc) {
        input_file = argv[optind++];
    }
    if (optind < argc) {
        output_file = argv[optind++];
    }
    
    if (!input_file || !output_file) {
        print_error("需要指定输入文件和输出文件");
        print_help();
        return 1;
    }
    
    if (!file_exists(input_file)) {
        print_error("输入文件不存在: %s", input_file);
        return 1;
    }
    
    // 执行转换
    if (conversion_type) {
        if (strcmp(conversion_type, "html") == 0) {
            color_print(COLOR_GREEN, "正在转换文本到HTML...");
            if (text_to_html(input_file, output_file)) {
                color_println(COLOR_GREEN, "完成");
                print_success("已保存到: %s", output_file);
                return 0;
            } else {
                print_error("转换失败");
                return 1;
            }
        }
        else if (strcmp(conversion_type, "json") == 0) {
            color_print(COLOR_GREEN, "正在转换文本到JSON...");
            if (text_to_json(input_file, output_file)) {
                color_println(COLOR_GREEN, "完成");
                print_success("已保存到: %s", output_file);
                return 0;
            } else {
                print_error("转换失败");
                return 1;
            }
        }
        else {
            print_error("不支持的转换类型: %s", conversion_type);
            return 1;
        }
    }
    else if (encoding) {
        color_print(COLOR_GREEN, "编码转换功能需要额外实现...");
        print_error("编码转换暂未实现");
        return 1;
    }
    else {
        print_error("请指定转换类型 (-t) 或编码 (-e)");
        return 1;
    }
    
    return 0;
}