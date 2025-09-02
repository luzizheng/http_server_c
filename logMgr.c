#include "logMgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>



int dlt_init_client(const char *app_id) {
    // 初始化日志客户端
    // 这里假设初始化总是成功的
    return 0;
}


int dlt_free_client(const char *app_id) {
    // 释放日志客户端资源
    return 0;
}


int dlt_log_fatal(const char *app_id, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[FATAL] [%s] ", app_id);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return 0;
}

int dlt_log_error(const char *app_id, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[ERROR] [%s] ", app_id);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return 0;
}

int dlt_log_warn(const char *app_id, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[WARN] [%s] ", app_id);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return 0;
}

int dlt_log_debug(const char *app_id, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, "[DEBUG] [%s] ", app_id);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    va_end(args);
    return 0;
}

int dlt_log_info(const char *app_id, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, "[INFO] [%s] ", app_id);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    va_end(args);
    return 0;
}

int dlt_log_verbose(const char *app_id, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, "[VERBOSE] [%s] ", app_id);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    va_end(args);
    return 0;
}


