#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "server.h"
#include "config.h"
#include "http_server.h"
#include "ftp_server.h"
#include "logMgr.h"

#define APP_ID "SRV"

// 全局配置
static ServerConfig server_config;
// 全局运行标志
volatile bool server_running = true;

// 信号处理函数，用于退出
void handle_signal(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nReceived termination signal. Shutting down servers...\n");
        server_running = false;
    }
}






// 打印使用帮助
void print_usage(const char *prog_name) {
    printf("Usage: %s [-c config_file]\n", prog_name);
    printf("  -c config_file   Specify configuration file (default: /etc/server.conf)\n");
}


int main(int argc, char *argv[]) {
    pthread_t http_thread, ftp_thread;
    int ret;
    const char *config_file = "/etc/server.conf";
    ThreadData thread_data;

    // 初始化日志模块
    if (dlt_init_client(APP_ID) != 0) {
        fprintf(stderr, "[ERROR] Failed to initialize logging module.\n");
    } else {
        dlt_log_debug(APP_ID, "Logging module initialized.");
    }

    // 解析命令行参数

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_file = argv[i + 1];
            dlt_log_debug(APP_ID, "Using config file: %s", config_file);
            i++; // 跳过配置文件路径参数
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            dlt_log_warn(APP_ID, "Invalid argument: %s", argv[i]);
            fprintf(stderr, "Invalid argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // 加载配置文件

    ret = config_load(&server_config, config_file);
    if (ret != 0) {
        if (ret == -2) {
            dlt_log_warn(APP_ID, "Could not open config file %s, using default configuration", config_file);
            fprintf(stderr, "Warning: Could not open config file %s, using default configuration\n", config_file);
        } else {
            dlt_log_warn(APP_ID, "Error parsing config file %s, using default configuration", config_file);
            fprintf(stderr, "Warning: Error parsing config file %s, using default configuration\n", config_file);
        }
        config_init(&server_config); // 使用默认配置
    } else {
        dlt_log_debug(APP_ID, "Successfully loaded configuration from %s", config_file);
        printf("Successfully loaded configuration from %s\n", config_file);
    }

    // 打印配置信息

    dlt_log_debug(APP_ID, "Printing loaded configuration");
    config_print(&server_config);

    // 设置信号处理

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    dlt_log_debug(APP_ID, "Signal handlers set for SIGINT and SIGTERM");


    dlt_log_debug(APP_ID, "Starting multi-protocol server threads");
    printf("\nStarting multi-protocol server...\n");

    // 准备线程数据

    thread_data.config = &server_config;

    // 创建HTTP服务器线程

    ret = pthread_create(&http_thread, NULL, run_http_server, &thread_data);
    if (ret != 0) {
        dlt_log_error(APP_ID, "Failed to create HTTP server thread: %d", ret);
        fprintf(stderr, "Failed to create HTTP server thread: %d\n", ret);
        dlt_free_client(APP_ID);
        exit(EXIT_FAILURE);
    } else {
        dlt_log_debug(APP_ID, "HTTP server thread created successfully");
    }

    // 创建FTP服务器线程

    ret = pthread_create(&ftp_thread, NULL, run_ftp_server, &thread_data);
    if (ret != 0) {
        dlt_log_error(APP_ID, "Failed to create FTP server thread: %d", ret);
        fprintf(stderr, "Failed to create FTP server thread: %d\n", ret);
        // 如果FTP线程创建失败，需要关闭HTTP线程
        server_running = false;
        pthread_join(http_thread, NULL);
        dlt_free_client(APP_ID);
        exit(EXIT_FAILURE);
    } else {
        dlt_log_debug(APP_ID, "FTP server thread created successfully");
    }

    // 等待线程结束
    pthread_join(http_thread, NULL);
    dlt_log_debug(APP_ID, "HTTP server thread exited");
    pthread_join(ftp_thread, NULL);
    dlt_log_debug(APP_ID, "FTP server thread exited");

    dlt_log_debug(APP_ID, "All servers stopped. Exiting.");
    printf("All servers stopped. Exiting.\n");

    // 关闭日志模块
    dlt_free_client(APP_ID);
    return 0;
}
