#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define SERVER_CONFIG_FILE "/etc/server.conf"

#define SERVER_DEFAULT_HTTP_IP       "0.0.0.0"
#define SERVER_DEFAULT_HTTP_PORT     8081
#define SERVER_DEFAULT_HTTP_ROOT     "/tmp/httproot"
#define SERVER_DEFAULT_HTTP_MAX_CONN 50

#define SERVER_DEFAULT_FTP_IP           "0.0.0.0"
#define SERVER_DEFAULT_FTP_PORT         21
#define SERVER_DEFAULT_FTP_ROOT         "/tmp/ftproot"
#define SERVER_DEFAULT_FTP_MAX_CONN     20
#define SERVER_DEFAULT_FTP_DATA_PORT_MIN 2000
#define SERVER_DEFAULT_FTP_DATA_PORT_MAX 2100   



// HTTP服务器配置结构体
typedef struct {
    char ip[16];           // IP地址，如"127.0.0.1"或"0.0.0.0"
    uint16_t port;         // 端口号
    char root_dir[256];    // 根目录路径
    int max_connections;   // 最大连接数
} HttpServerConfig;

// FTP服务器配置结构体
typedef struct {
    char ip[16];           // IP地址
    uint16_t port;         // 端口号
    char root_dir[256];    // 根目录路径
    int max_connections;   // 最大连接数
    int data_port_min;     // 数据传输端口范围最小值
    int data_port_max;     // 数据传输端口范围最大值
} FtpServerConfig;

// 全局配置结构体
typedef struct {
    HttpServerConfig http; // HTTP服务器配置
    FtpServerConfig ftp;   // FTP服务器配置
} ServerConfig;

// 线程数据结构，用于向线程传递配置参数
typedef struct {
    ServerConfig *config;
} ThreadData;


// 初始化配置为默认值
void config_init(ServerConfig *config);

// 从文件加载配置
// 返回值：0表示成功，非0表示失败
int config_load(ServerConfig *config, const char *filename);

// 打印配置信息（用于调试）
void config_print(const ServerConfig *config);

#endif // CONFIG_H
