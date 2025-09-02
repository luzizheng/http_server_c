#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include "server.h"

// 全局运行标志
volatile bool server_running = true;

// 信号处理函数，用于优雅退出
void handle_signal(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nReceived termination signal. Shutting down servers...\n");
        server_running = false;
    }
}

// 主函数，创建并管理两个服务器线程
int main() {
    pthread_t http_thread, ftp_thread;
    int ret;

    // 设置信号处理
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting multi-protocol server...\n");

    // 创建HTTP服务器线程
    ret = pthread_create(&http_thread, NULL, run_http_server, NULL);
    if (ret != 0) {
        fprintf(stderr, "Failed to create HTTP server thread: %d\n", ret);
        exit(EXIT_FAILURE);
    }

    // 创建FTP服务器线程
    ret = pthread_create(&ftp_thread, NULL, run_ftp_server, NULL);
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
