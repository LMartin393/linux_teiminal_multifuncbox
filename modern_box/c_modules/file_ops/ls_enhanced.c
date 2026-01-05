#include "ls_enhanced.h"
#include "../utils/ansi_unicode.h"
#include "../utils/common_utils.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <git2.h>  // libgit2：获取git状态

// 获取文件类型
FileType get_file_type(const char* path) {
    struct stat st;
    if (lstat(path, &st) < 0) return FILE_REGULAR;
    if (S_ISDIR(st.st_mode)) return FILE_DIR;
    if (S_ISLNK(st.st_mode)) return FILE_LINK;
    if (S_IXUSR & st.st_mode) return FILE_EXEC;
    // 简单判断文件后缀（图片/视频/文本）
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0) return FILE_IMAGE;
        if (strcmp(ext, ".mp4") == 0 || strcmp(ext, ".avi") == 0) return FILE_VIDEO;
        if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 || strcmp(ext, ".py") == 0) return FILE_TEXT;
    }
    return FILE_REGULAR;
}

// 获取Git文件状态
GitStatus get_git_status(const char* file_path) {
    git_repository *repo = NULL;
    git_status_list *status_list = NULL;
    const git_status_entry *entry;
    GitStatus status = GIT_UNTRACKED;

    // 打开当前目录的git仓库
    if (git_repository_open_ext(&repo, ".", 0, NULL) != 0) {
        return GIT_COMMITTED; // 非git仓库
    }

    // 获取状态列表
    if (git_status_list_new(&status_list, repo, NULL) != 0) {
        git_repository_free(repo);
        return GIT_UNTRACKED;
    }

    // 遍历状态查找目标文件
    for (size_t i = 0; i < git_status_list_entrycount(status_list); i++) {
        entry = git_status_byindex(status_list, i);
        if (strcmp(entry->path, file_path) == 0) {
            if (entry->status & GIT_STATUS_INDEX_MODIFIED || entry->status & GIT_STATUS_WT_MODIFIED) {
                status = GIT_MODIFIED;
            } else if (entry->status & GIT_STATUS_INDEX_ADDED) {
                status = GIT_ADDED;
            } else if (entry->status & GIT_STATUS_INDEX_DELETED || entry->status & GIT_STATUS_WT_DELETED) {
                status = GIT_DELETED;
            }
            break;
        }
    }

    // 释放资源
    git_status_list_free(status_list);
    git_repository_free(repo);
    return status;
}

// 增强版ls主函数
void ls_enhanced(const char* dir_path) {
    DIR *dir;
    struct dirent *entry;
    char full_path[PATH_MAX];

    // 打开目录
    dir = opendir(dir_path);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    // 启用ANSI颜色
    ansi_enable(1);

    // 遍历目录项
    while ((entry = readdir(dir)) != NULL) {
        // 跳过隐藏文件（可通过参数控制是否显示）
        if (entry->d_name[0] == '.') continue;

        // 拼接完整路径
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        // 获取文件类型和Git状态
        FileType f_type = get_file_type(full_path);
        GitStatus g_status = get_git_status(entry->d_name);

        // 输出：图标 + 彩色文件名 + Git状态提示
        print_unicode(unicode_icon(f_type));
        printf(" ");
        printf("%s%s%s ", ansi_color(f_type, g_status), entry->d_name, ANSI_RESET);
        if (g_status != GIT_COMMITTED && g_status != GIT_UNTRACKED) {
            printf("(%s)", git_status_str(g_status));
        }
        printf("\n");
    }

    closedir(dir);
}

// Git状态转字符串
const char* git_status_str(GitStatus status) {
    switch (status) {
        case GIT_MODIFIED: return "modified";
        case GIT_ADDED: return "added";
        case GIT_DELETED: return "deleted";
        default: return "";
    }
}