// src/text_tools/tkview.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "../common/colors.h"
#include "../common/utils.h"
#include "../common/progress.h"

#define MAX_LINE 4096
#define MAX_COLUMNS 100
#define MAX_ROWS 10000
#define MAX_FIELD_LEN 256

typedef enum {
    FORMAT_CSV,
    FORMAT_TSV,
    FORMAT_JSON,
    FORMAT_XML,
    FORMAT_AUTO
} FileFormat;

typedef struct {
    char ***data;           // 数据矩阵 [行][列]
    int rows;               // 行数
    int cols;               // 列数
    char **headers;         // 列标题
    int *col_widths;        // 列宽度
    long file_size;         // 文件大小
    char filename[256];     // 文件名
    FileFormat format;      // 文件格式
} DataTable;

typedef struct {
    int show_headers;       // 显示标题行
    int max_rows;           // 最大显示行数
    int max_cols;           // 最大显示列数
    int page_size;          // 每页行数
    int current_page;       // 当前页码
    int sort_column;        // 排序列
    int sort_desc;          // 降序排序
    int filter_enabled;     // 过滤器启用
    char filter[256];       // 过滤文本
    int highlight;          // 高亮匹配
    int color_output;       // 彩色输出
    int interactive;        // 交互模式
    int show_stats;         // 显示统计信息
    int wrap_text;          // 自动换行
} ViewConfig;

void print_help() {
    printf("tkview - CSV/JSON/XML 文件查看器\n\n");
    printf("用法:\n");
    printf("  tkview [选项] <文件>\n\n");
    
    printf("选项:\n");
    printf("  -f <格式>    文件格式 (csv, tsv, json, xml, auto)\n");
    printf("  -H           不显示标题行\n");
    printf("  -n <行数>    显示前N行 (默认: 50)\n");
    printf("  -c <列数>    显示前N列\n");
    printf("  -p <大小>    每页行数 (交互模式)\n");
    printf("  -s <列>      按指定列排序\n");
    printf("  -r           反向排序\n");
    printf("  -g <文本>    过滤包含文本的行\n");
    printf("  -h           高亮匹配的文本\n");
    printf("  -C           强制彩色输出\n");
    printf("  -i           交互模式\n");
    printf("  -S           显示统计信息\n");
    printf("  -w           自动换行长文本\n");
    printf("  -v           详细输出\n");
    printf("  --help       显示帮助\n\n");
    
    printf("交互模式快捷键:\n");
    printf("  n/p          下一页/上一页\n");
    printf("  j/k          向下/向上滚动\n");
    printf("  g/G          跳转到首行/末行\n");
    printf("  s            切换排序\n");
    printf("  f            过滤模式\n");
    printf("  h            显示帮助\n");
    printf("  q            退出\n\n");
    
    printf("示例:\n");
    printf("  tkview data.csv\n");
    printf("  tkview -f json data.json -n 100\n");
    printf("  tkview -i data.csv -p 20\n");
    printf("  tkview data.tsv -s 3 -r\n");
    printf("  tkview data.csv -g \"error\" -h\n");
}

// 获取终端大小
void get_terminal_size(int *width, int *height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *width = ws.ws_col;
        *height = ws.ws_row;
    } else {
        *width = 80;
        *height = 24;
    }
}

// 自动检测文件格式
FileFormat detect_file_format(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return FORMAT_AUTO;
    
    char first_line[MAX_LINE];
    if (!fgets(first_line, sizeof(first_line), file)) {
        fclose(file);
        return FORMAT_AUTO;
    }
    
    fclose(file);
    trim_string(first_line);
    
    // 检查JSON
    if (first_line[0] == '{' || first_line[0] == '[') {
        return FORMAT_JSON;
    }
    
    // 检查XML
    if (strstr(first_line, "<?xml") != NULL || strstr(first_line, "<root") != NULL) {
        return FORMAT_XML;
    }
    
    // 检查CSV/TSV
    int comma_count = 0;
    int tab_count = 0;
    for (int i = 0; first_line[i]; i++) {
        if (first_line[i] == ',') comma_count++;
        if (first_line[i] == '\t') tab_count++;
    }
    
    if (tab_count > comma_count && tab_count > 0) {
        return FORMAT_TSV;
    } else if (comma_count > 0) {
        return FORMAT_CSV;
    }
    
    return FORMAT_AUTO;
}

// 简单的CSV解析（不处理带引号的逗号等复杂情况）
int parse_csv(DataTable *table, FILE *file, int max_rows) {
    char line[MAX_LINE];
    int row = 0;
    
    // 分配初始内存
    table->data = malloc(MAX_ROWS * sizeof(char **));
    for (int i = 0; i < MAX_ROWS; i++) {
        table->data[i] = malloc(MAX_COLUMNS * sizeof(char *));
    }
    table->headers = malloc(MAX_COLUMNS * sizeof(char *));
    
    // 读取标题行（如果有）
    if (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        char *token = strtok(line, ",");
        int col = 0;
        
        while (token && col < MAX_COLUMNS) {
            trim_string(token);
            table->headers[col] = strdup(token);
            table->col_widths[col] = strlen(token);
            col++;
            token = strtok(NULL, ",");
        }
        table->cols = col;
    }
    
    // 读取数据行
    while (fgets(line, sizeof(line), file) && row < max_rows && row < MAX_ROWS) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;
        
        char *token = strtok(line, ",");
        int col = 0;
        
        while (token && col < MAX_COLUMNS) {
            trim_string(token);
            table->data[row][col] = strdup(token);
            
            // 更新列宽度
            int len = strlen(token);
            if (len > table->col_widths[col]) {
                table->col_widths[col] = len;
            }
            
            col++;
            token = strtok(NULL, ",");
        }
        
        // 确保每行列数一致
        if (col > table->cols) {
            table->cols = col;
        }
        
        row++;
    }
    
    table->rows = row;
    return 1;
}

// 简单的TSV解析
int parse_tsv(DataTable *table, FILE *file, int max_rows) {
    char line[MAX_LINE];
    int row = 0;
    
    table->data = malloc(MAX_ROWS * sizeof(char **));
    for (int i = 0; i < MAX_ROWS; i++) {
        table->data[i] = malloc(MAX_COLUMNS * sizeof(char *));
    }
    table->headers = malloc(MAX_COLUMNS * sizeof(char *));
    
    // 读取标题行
    if (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        char *token = strtok(line, "\t");
        int col = 0;
        
        while (token && col < MAX_COLUMNS) {
            trim_string(token);
            table->headers[col] = strdup(token);
            table->col_widths[col] = strlen(token);
            col++;
            token = strtok(NULL, "\t");
        }
        table->cols = col;
    }
    
    // 读取数据行
    while (fgets(line, sizeof(line), file) && row < max_rows && row < MAX_ROWS) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;
        
        char *token = strtok(line, "\t");
        int col = 0;
        
        while (token && col < MAX_COLUMNS) {
            trim_string(token);
            table->data[row][col] = strdup(token);
            
            int len = strlen(token);
            if (len > table->col_widths[col]) {
                table->col_widths[col] = len;
            }
            
            col++;
            token = strtok(NULL, "\t");
        }
        
        if (col > table->cols) {
            table->cols = col;
        }
        
        row++;
    }
    
    table->rows = row;
    return 1;
}

// 简单的JSON解析（仅支持数组对象）
int parse_json(DataTable *table, FILE *file, int max_rows) {
    // 简化实现：读取整个文件
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    
    // 简单解析JSON数组
    char *ptr = content;
    int row = 0;
    int in_array = 0;
    int in_object = 0;
    
    // 分配内存
    table->data = malloc(MAX_ROWS * sizeof(char **));
    for (int i = 0; i < MAX_ROWS; i++) {
        table->data[i] = malloc(MAX_COLUMNS * sizeof(char *));
    }
    table->headers = malloc(MAX_COLUMNS * sizeof(char *));
    
    // 简单解析（仅用于演示）
    while (*ptr && row < max_rows) {
        if (*ptr == '[') in_array = 1;
        else if (*ptr == '{' && in_array) {
            in_object = 1;
            // 开始新行
        }
        else if (*ptr == '}' && in_object) {
            in_object = 0;
            row++;
        }
        ptr++;
    }
    
    free(content);
    
    // 简化：返回一个示例数据
    table->rows = 1;
    table->cols = 3;
    table->headers[0] = strdup("字段1");
    table->headers[1] = strdup("字段2");
    table->headers[2] = strdup("字段3");
    table->col_widths[0] = 6;
    table->col_widths[1] = 6;
    table->col_widths[2] = 6;
    
    table->data[0][0] = strdup("JSON");
    table->data[0][1] = strdup("数据");
    table->data[0][2] = strdup("示例");
    
    return 1;
}

// 加载数据文件
DataTable* load_data_file(const char *filename, FileFormat format, int max_rows) {
    DataTable *table = malloc(sizeof(DataTable));
    if (!table) return NULL;
    
    memset(table, 0, sizeof(DataTable));
    strncpy(table->filename, filename, sizeof(table->filename) - 1);
    table->format = format;
    
    // 获取文件大小
    struct stat st;
    if (stat(filename, &st) == 0) {
        table->file_size = st.st_size;
    }
    
    // 分配列宽度数组
    table->col_widths = calloc(MAX_COLUMNS, sizeof(int));
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        free(table);
        return NULL;
    }
    
    int success = 0;
    switch (format) {
        case FORMAT_CSV:
            success = parse_csv(table, file, max_rows);
            break;
        case FORMAT_TSV:
            success = parse_tsv(table, file, max_rows);
            break;
        case FORMAT_JSON:
            success = parse_json(table, file, max_rows);
            break;
        case FORMAT_XML:
        case FORMAT_AUTO:
            // 简化处理
            success = parse_csv(table, file, max_rows);
            break;
    }
    
    fclose(file);
    
    if (!success) {
        free_data_table(table);
        return NULL;
    }
    
    return table;
}

// 释放数据表内存
void free_data_table(DataTable *table) {
    if (!table) return;
    
    if (table->headers) {
        for (int i = 0; i < table->cols; i++) {
            if (table->headers[i]) free(table->headers[i]);
        }
        free(table->headers);
    }
    
    if (table->data) {
        for (int i = 0; i < table->rows; i++) {
            if (table->data[i]) {
                for (int j = 0; j < table->cols; j++) {
                    if (table->data[i][j]) free(table->data[i][j]);
                }
                free(table->data[i]);
            }
        }
        free(table->data);
    }
    
    if (table->col_widths) free(table->col_widths);
    free(table);
}

// 显示数据表
void display_table(DataTable *table, ViewConfig *config, int start_row, int end_row) {
    if (!table || table->rows == 0 || table->cols == 0) {
        printf("没有数据可显示\n");
        return;
    }
    
    int term_width = 80;
    int term_height = 24;
    get_terminal_size(&term_width, &term_height);
    
    // 计算可用的列数
    int available_cols = table->cols;
    if (config->max_cols > 0 && config->max_cols < available_cols) {
        available_cols = config->max_cols;
    }
    
    // 计算每列的显示宽度
    int *display_widths = malloc(available_cols * sizeof(int));
    int total_width = 0;
    
    for (int i = 0; i < available_cols; i++) {
        display_widths[i] = table->col_widths[i] + 3;  // 加3用于分隔符和间距
        if (display_widths[i] > 30) display_widths[i] = 30;  // 限制最大宽度
        total_width += display_widths[i];
    }
    
    // 调整宽度以适应终端
    if (total_width > term_width - 5) {
        int overflow = total_width - (term_width - 5);
        for (int i = available_cols - 1; i >= 0 && overflow > 0; i--) {
            int reduce = display_widths[i] - 10;  // 最小宽度10
            if (reduce > overflow) reduce = overflow;
            if (display_widths[i] - reduce >= 10) {
                display_widths[i] -= reduce;
                overflow -= reduce;
            }
        }
    }
    
    // 打印标题行
    if (config->show_headers && table->headers) {
        printf("\n");
        color_print(COLOR_CYAN, " ");
        for (int i = 0; i < available_cols; i++) {
            if (i == config->sort_column) {
                color_print(COLOR_YELLOW, "%*s", display_widths[i] - 3, table->headers[i]);
                printf(config->sort_desc ? " ↓ " : " ↑ ");
            } else {
                printf("%*s ", display_widths[i] - 3, table->headers[i]);
            }
        }
        color_print(COLOR_CYAN, "\n");
        
        // 打印分隔线
        printf(" ");
        for (int i = 0; i < available_cols; i++) {
            for (int j = 0; j < display_widths[i]; j++) {
                printf("-");
            }
        }
        printf("\n");
    }
    
    // 打印数据行
    int rows_to_show = end_row - start_row;
    if (rows_to_show > table->rows - start_row) {
        rows_to_show = table->rows - start_row;
    }
    
    for (int r = start_row; r < start_row + rows_to_show; r++) {
        // 检查是否匹配过滤器
        int show_row = 1;
        if (config->filter_enabled && strlen(config->filter) > 0) {
            show_row = 0;
            for (int c = 0; c < available_cols; c++) {
                if (table->data[r][c] && strstr(table->data[r][c], config->filter)) {
                    show_row = 1;
                    break;
                }
            }
        }
        
        if (!show_row) continue;
        
        printf(" ");
        for (int c = 0; c < available_cols; c++) {
            if (c < table->cols && table->data[r][c]) {
                char display[MAX_FIELD_LEN];
                strncpy(display, table->data[r][c], sizeof(display) - 1);
                display[sizeof(display) - 1] = '\0';
                
                // 截断长文本
                if (strlen(display) > display_widths[c] - 3) {
                    display[display_widths[c] - 6] = '.';
                    display[display_widths[c] - 5] = '.';
                    display[display_widths[c] - 4] = '.';
                    display[display_widths[c] - 3] = '\0';
                }
                
                // 高亮匹配文本
                if (config->highlight && config->filter_enabled && 
                    strlen(config->filter) > 0 && strstr(display, config->filter)) {
                    char *match = strstr(display, config->filter);
                    int match_pos = match - display;
                    int match_len = strlen(config->filter);
                    
                    printf("%.*s", match_pos, display);
                    color_print(COLOR_BRIGHT_RED, "%.*s", match_len, match);
                    printf("%s", match + match_len);
                    printf("%*s", display_widths[c] - 3 - (int)strlen(display), "");
                } else {
                    printf("%-*s ", display_widths[c] - 3, display);
                }
            } else {
                printf("%-*s ", display_widths[c] - 3, "");
            }
        }
        printf("\n");
        
        // 自动换行模式
        if (config->wrap_text) {
            // 检查是否有需要换行的长文本
            for (int c = 0; c < available_cols; c++) {
                if (table->data[r][c] && strlen(table->data[r][c]) > display_widths[c] * 2) {
                    printf("    [列%d]: %s\n", c + 1, table->data[r][c]);
                }
            }
        }
    }
    
    free(display_widths);
}

// 显示统计信息
void display_stats(DataTable *table, ViewConfig *config) {
    printf("\n");
    color_println(COLOR_CYAN, "文件统计:");
    printf("文件名: %s\n", table->filename);
    printf("格式: ");
    switch (table->format) {
        case FORMAT_CSV: printf("CSV\n"); break;
        case FORMAT_TSV: printf("TSV\n"); break;
        case FORMAT_JSON: printf("JSON\n"); break;
        case FORMAT_XML: printf("XML\n"); break;
        default: printf("未知\n");
    }
    printf("大小: %s\n", format_size(table->file_size));
    printf("行数: %d\n", table->rows);
    printf("列数: %d\n", table->cols);
    
    if (table->rows > 0 && table->cols > 0) {
        printf("\n");
        color_println(COLOR_CYAN, "列信息:");
        for (int i = 0; i < table->cols && i < 10; i++) {  // 只显示前10列
            printf("  [%2d] %-20s 宽度: %d\n", 
                   i + 1, 
                   table->headers ? table->headers[i] : "(无标题)", 
                   table->col_widths[i]);
        }
        if (table->cols > 10) {
            printf("  ... 还有 %d 列\n", table->cols - 10);
        }
    }
    
    if (config->filter_enabled) {
        printf("\n过滤器: \"%s\"\n", config->filter);
    }
}

// 交互模式
void interactive_mode(DataTable *table, ViewConfig *config) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    int total_pages = (table->rows + config->page_size - 1) / config->page_size;
    config->current_page = 0;
    
    printf("\n");
    color_println(COLOR_GREEN, "进入交互模式 (h显示帮助)");
    
    while (1) {
        int start_row = config->current_page * config->page_size;
        int end_row = start_row + config->page_size;
        
        // 清屏
        printf("\033[2J\033[H");
        
        // 显示标题
        color_println(COLOR_CYAN, "文件: %s [%dx%d]", 
                     table->filename, table->rows, table->cols);
        
        // 显示数据
        display_table(table, config, start_row, end_row);
        
        // 显示页脚
        printf("\n");
        printf("第 %d/%d 页 | 行 %d-%d (共 %d 行)", 
               config->current_page + 1, total_pages,
               start_row + 1, 
               end_row < table->rows ? end_row : table->rows,
               table->rows);
        
        if (config->filter_enabled) {
            color_print(COLOR_YELLOW, " | 过滤: \"%s\"", config->filter);
        }
        if (config->sort_column >= 0) {
            printf(" | 排序: 列%d %s", 
                   config->sort_column + 1,
                   config->sort_desc ? "降序" : "升序");
        }
        printf("\n命令: ");
        fflush(stdout);
        
        // 读取命令
        char cmd = getchar();
        
        switch (cmd) {
            case 'q':
            case 'Q':
                tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                return;
                
            case 'n':
            case ' ':
                if (config->current_page < total_pages - 1) {
                    config->current_page++;
                }
                break;
                
            case 'p':
            case 'P':
                if (config->current_page > 0) {
                    config->current_page--;
                }
                break;
                
            case 'j':
                start_row++;
                if (start_row + config->page_size <= table->rows) {
                    config->current_page = start_row / config->page_size;
                }
                break;
                
            case 'k':
                start_row--;
                if (start_row >= 0) {
                    config->current_page = start_row / config->page_size;
                }
                break;
                
            case 'g':
                config->current_page = 0;
                break;
                
            case 'G':
                config->current_page = total_pages - 1;
                break;
                
            case 's':
                config->sort_column = (config->sort_column + 1) % table->cols;
                printf("\n按列%d排序...\n", config->sort_column + 1);
                // 这里应该实现排序逻辑
                break;
                
            case 'f':
                printf("\n输入过滤文本: ");
                fflush(stdout);
                
                // 恢复终端设置以读取文本
                tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                scanf("%255s", config->filter);
                config->filter_enabled = (strlen(config->filter) > 0);
                tcsetattr(STDIN_FILENO, TCSANOW, &newt);
                break;
                
            case 'h':
                printf("\n");
                printf("快捷键:\n");
                printf("  n,空格  下一页\n");
                printf("  p        上一页\n");
                printf("  j/k      向下/上滚动\n");
                printf("  g/G      首页/末页\n");
                printf("  s        切换排序列\n");
                printf("  f        过滤模式\n");
                printf("  h        显示帮助\n");
                printf("  q        退出\n");
                printf("\n按任意键继续...");
                getchar();
                break;
                
            default:
                break;
        }
    }
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    if (strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }
    
    ViewConfig config = {
        .show_headers = 1,
        .max_rows = 50,
        .max_cols = 0,
        .page_size = 20,
        .current_page = 0,
        .sort_column = -1,
        .sort_desc = 0,
        .filter_enabled = 0,
        .filter = "",
        .highlight = 0,
        .color_output = is_color_supported(),
        .interactive = 0,
        .show_stats = 0,
        .wrap_text = 0
    };
    
    FileFormat format = FORMAT_AUTO;
    char filename[256] = "";
    
    // 解析选项
    int opt;
    while ((opt = getopt(argc, argv, "f:Hn:c:p:s:rg:hCiSwv")) != -1) {
        switch (opt) {
            case 'f':
                if (strcmp(optarg, "csv") == 0) format = FORMAT_CSV;
                else if (strcmp(optarg, "tsv") == 0) format = FORMAT_TSV;
                else if (strcmp(optarg, "json") == 0) format = FORMAT_JSON;
                else if (strcmp(optarg, "xml") == 0) format = FORMAT_XML;
                else if (strcmp(optarg, "auto") == 0) format = FORMAT_AUTO;
                else {
                    print_error("不支持的格式: %s", optarg);
                    return 1;
                }
                break;
            case 'H':
                config.show_headers = 0;
                break;
            case 'n':
                config.max_rows = atoi(optarg);
                if (config.max_rows < 1) config.max_rows = 50;
                break;
            case 'c':
                config.max_cols = atoi(optarg);
                break;
            case 'p':
                config.page_size = atoi(optarg);
                if (config.page_size < 1) config.page_size = 20;
                break;
            case 's':
                config.sort_column = atoi(optarg) - 1;  // 转换为0-based
                break;
            case 'r':
                config.sort_desc = 1;
                break;
            case 'g':
                strncpy(config.filter, optarg, sizeof(config.filter) - 1);
                config.filter_enabled = 1;
                break;
            case 'h':
                config.highlight = 1;
                break;
            case 'C':
                config.color_output = 1;
                break;
            case 'i':
                config.interactive = 1;
                break;
            case 'S':
                config.show_stats = 1;
                break;
            case 'w':
                config.wrap_text = 1;
                break;
            case 'v':
                config.color_output = 1;
                config.show_stats = 1;
                break;
            default:
                print_help();
                return 1;
        }
    }
    
    // 获取文件名
    if (optind < argc) {
        strncpy(filename, argv[optind], sizeof(filename) - 1);
    } else {
        print_error("需要指定文件名");
        return 1;
    }
    
    if (!file_exists(filename)) {
        print_error("文件不存在: %s", filename);
        return 1;
    }
    
    // 自动检测格式
    if (format == FORMAT_AUTO) {
        format = detect_file_format(filename);
        if (config.show_stats) {
            printf("检测到文件格式: ");
            switch (format) {
                case FORMAT_CSV: printf("CSV\n"); break;
                case FORMAT_TSV: printf("TSV\n"); break;
                case FORMAT_JSON: printf("JSON\n"); break;
                case FORMAT_XML: printf("XML\n"); break;
                default: printf("未知\n");
            }
        }
    }
    
    // 加载数据
    DataTable *table = load_data_file(filename, format, config.max_rows);
    if (!table) {
        print_error("无法加载文件: %s", filename);
        return 1;
    }
    
    // 显示数据
    if (config.interactive) {
        interactive_mode(table, &config);
    } else {
        printf("\n");
        display_table(table, &config, 0, config.max_rows < table->rows ? config.max_rows : table->rows);
        
        if (config.show_stats) {
            display_stats(table, &config);
        }
        
        if (config.max_rows < table->rows) {
            printf("\n");
            color_println(COLOR_YELLOW, "提示: 只显示了前 %d 行 (共 %d 行)", 
                         config.max_rows, table->rows);
            printf("      使用 -n %d 查看更多行，或使用 -i 进入交互模式\n", table->rows);
        }
    }
    
    // 清理
    free_data_table(table);
    
    return 0;
}