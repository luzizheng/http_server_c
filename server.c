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

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_file = argv[i + 1];
            i++; // 跳过配置文件路径参数
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Invalid argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // 加载配置文件
    ret = config_load(&server_config, config_file);
    if (ret != 0) {
        if (ret == -2) {
            fprintf(stderr, "Warning: Could not open config file %s, using default configuration\n", config_file);
        } else {
            fprintf(stderr, "Warning: Error parsing config file %s, using default configuration\n", config_file);
        }
        config_init(&server_config); // 使用默认配置
    } else {
        printf("Successfully loaded configuration from %s\n", config_file);
    }

    // 打印配置信息
    config_print(&server_config);

    // 设置信号处理
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("\nStarting multi-protocol server...\n");

    // 准备线程数据
    thread_data.config = &server_config;

    // 创建HTTP服务器线程
    ret = pthread_create(&http_thread, NULL, run_http_server, &thread_data);
    if (ret != 0) {
        fprintf(stderr, "Failed to create HTTP server thread: %d\n", ret);
        exit(EXIT_FAILURE);
    }

    // 创建FTP服务器线程
    ret = pthread_create(&ftp_thread, NULL, run_ftp_server, &thread_data);
    if (ret != 0) {
        fprintf(stderr, "Failed to create FTP server thread: %d\n", ret);
        // 如果FTP线程创建失败，需要关闭HTTP线程
        server_running = false;
        pthread_join(http_thread, NULL);
        exit(EXIT_FAILURE);
    }

    // 等待线程结束
    pthread_join(http_thread, NULL);
    pthread_join(ftp_thread, NULL);

    printf("All servers stopped. Exiting.\n");
    return 0;
}
