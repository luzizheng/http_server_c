
#define UTILS_PATHBUF_SIZE 4096

#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

int safe_path_join(char *dest, size_t dest_size, const char *path1, const char *path2, const char *separator) {
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    size_t sep_len = separator ? strlen(separator) : 0;
    if (len1 + sep_len + len2 + 1 > dest_size) {
        return 0;
    }
    strcpy(dest, path1);
    if (separator && sep_len > 0) {
        strcat(dest, separator);
    }
    strcat(dest, path2);
    return 1;
}

int is_path_safe(const char *path, const char *root_dir) {
    char real_root[UTILS_PATHBUF_SIZE];
    char real_path[UTILS_PATHBUF_SIZE];
    if (realpath(root_dir, real_root) == NULL) {
        perror("realpath");
        return 0;
    }
    if (realpath(path, real_path) == NULL) {
        return 0;
    }
    return strstr(real_path, real_root) == real_path;
}

const char* get_file_size_str(off_t size) {
    static char str[32];
    if (size < 1024) {
        snprintf(str, sizeof(str), "%ld B", (long)size);
    } else if (size < 1024 * 1024) {
        snprintf(str, sizeof(str), "%.1f KB", (double)size / 1024);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(str, sizeof(str), "%.1f MB", (double)size / (1024 * 1024));
    } else {
        snprintf(str, sizeof(str), "%.1f GB", (double)size / (1024 * 1024 * 1024));
    }
    return str;
}
