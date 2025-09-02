#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>

// 全局运行标志，用于控制线程退出
extern volatile bool server_running;

// 信号处理函数，用于优雅退出
void handle_signal(int signum);

// HTTP服务器函数，供线程调用
void *run_http_server(void *arg);

// FTP服务器函数声明，实际实现在ftp_server.c中
void *run_ftp_server(void *arg);

#endif // SERVER_H
