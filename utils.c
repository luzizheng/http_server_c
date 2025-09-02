
#define UTILS_PATHBUF_SIZE 4096

#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "logMgr.h"
#define APP_ID "SRV"

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


void print_raw_data(const char *prefix, const char *data, size_t len) {
    dlt_log_info(APP_ID, "%s: Raw data (%zu bytes):", prefix,len);
    dlt_log_info(APP_ID, "%s", data);
}


char *get_local_ip()
{
    static char ip[16] = "127.0.0.1";
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return ip;
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        family = ifa->ifa_addr->sa_family;
        
        // 仅处理IPv4地址
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST,
                            NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                dlt_log_error(APP_ID, "getnameinfo() failed: %s", gai_strerror(s));
                continue;   
            }
            // 排除回环地址
            if (strcmp(host, "127.0.0.1") == 0){
                continue;
            }

            // 使用第一个非回环地址
            strncpy(ip, host, sizeof(ip) - 1);
            break;
        }

    }
    freeifaddrs(ifaddr);
    return ip;
}


long get_file_size(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    } else {
        return -1; 
    }
}