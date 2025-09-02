#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

// 去除字符串首尾的空白字符
static char* trim_whitespace(char *str) {
    if (str == NULL) return NULL;
    
    // 去除开头的空白字符
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    
    // 如果全是空白字符，返回空字符串
    if (*start == '\0') {
        *str = '\0';
        return str;
    }
    
    // 去除结尾的空白字符
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    
    // 添加上字符串结束符
    *(end + 1) = '\0';
    
    // 如果起始位置有移动，将内容前移
    if (start != str) {
        memmove(str, start, end - start + 2);
    }
    
    return str;
}

// 解析键值对
#include "logMgr.h"
#define APP_ID "SRV"
static void parse_key_value(const char *line, char *section, 
                           HttpServerConfig *http, FtpServerConfig *ftp) {
    char key[128], value[256];
    char *equal_sign = strchr(line, '=');
    

    // 不包含等号，不是有效的键值对
    if (equal_sign == NULL) {
        dlt_log_debug(APP_ID, "Line is not a key-value pair: %s", line);
        return;
    }
    
    // 提取键
    size_t key_len = equal_sign - line;
    if (key_len >= sizeof(key)) {
        key_len = sizeof(key) - 1;
    }
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    trim_whitespace(key);
    
    // 提取值
    const char *value_start = equal_sign + 1;
    strncpy(value, value_start, sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';
    trim_whitespace(value);
    
    // 根据当前section解析配置
    if (strcmp(section, "http_server") == 0) {
        dlt_log_debug(APP_ID, "[http_server] %s = %s", key, value);
        if (strcmp(key, "ip") == 0) {
            strncpy(http->ip, value, sizeof(http->ip) - 1);
        } else if (strcmp(key, "port") == 0) {
            http->port = (uint16_t)atoi(value);
        } else if (strcmp(key, "root_dir") == 0) {
            strncpy(http->root_dir, value, sizeof(http->root_dir) - 1);
        } else if (strcmp(key, "max_connections") == 0) {
            http->max_connections = atoi(value);
        }
    } else if (strcmp(section, "ftp_server") == 0) {
        dlt_log_debug(APP_ID, "[ftp_server] %s = %s", key, value);
        if (strcmp(key, "ip") == 0) {
            strncpy(ftp->ip, value, sizeof(ftp->ip) - 1);
        } else if (strcmp(key, "port") == 0) {
            ftp->port = (uint16_t)atoi(value);
        } else if (strcmp(key, "root_dir") == 0) {
            strncpy(ftp->root_dir, value, sizeof(ftp->root_dir) - 1);
        } else if (strcmp(key, "max_connections") == 0) {
            ftp->max_connections = atoi(value);
        } else if (strcmp(key, "data_port_range") == 0) {
            char *dash = strchr(value, '-');
            if (dash != NULL) {
                *dash = '\0';
                ftp->data_port_min = atoi(value);
                ftp->data_port_max = atoi(dash + 1);
            }
        }
    }
}

// 初始化配置为默认值
void config_init(ServerConfig *config) {
    if (config == NULL) return;
    
    // HTTP服务器默认配置
    strcpy(config->http.ip, SERVER_DEFAULT_HTTP_IP);
    config->http.port = SERVER_DEFAULT_HTTP_PORT;
    strcpy(config->http.root_dir, SERVER_DEFAULT_HTTP_ROOT);
    config->http.max_connections = SERVER_DEFAULT_HTTP_MAX_CONN;
    
    // FTP服务器默认配置
    strcpy(config->ftp.ip, SERVER_DEFAULT_FTP_IP);
    config->ftp.port = SERVER_DEFAULT_FTP_PORT;
    strcpy(config->ftp.root_dir, SERVER_DEFAULT_FTP_ROOT);
    config->ftp.max_connections = SERVER_DEFAULT_FTP_MAX_CONN;
    config->ftp.data_port_min = SERVER_DEFAULT_FTP_DATA_PORT_MIN;
    config->ftp.data_port_max = SERVER_DEFAULT_FTP_DATA_PORT_MAX;
}

// 从文件加载配置
int config_load(ServerConfig *config, const char *filename) {
    if (config == NULL || filename == NULL) {
        dlt_log_error(APP_ID, "config_load: config or filename is NULL");
        return -1;
    }

    // 先初始化默认配置
    config_init(config);
    dlt_log_debug(APP_ID, "Default configuration initialized");

    // 打开配置文件
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        dlt_log_warn(APP_ID, "config_load: failed to open file %s", filename);
        return -2; // 文件打开失败
    }

    char line[512];
    char current_section[64] = "";

    // 逐行读取并解析
    while (fgets(line, sizeof(line), file) != NULL) {
        // 去除换行符
        char *newline = strchr(line, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        // 处理注释
        char *comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }

        // 去除首尾空白
        trim_whitespace(line);

        // 空行跳过
        if (line[0] == '\0') {
            continue;
        }

        // 处理section
        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            // 提取section名称
            size_t section_len = strlen(line) - 2;
            if (section_len < sizeof(current_section) - 1) {
                strncpy(current_section, line + 1, section_len);
                current_section[section_len] = '\0';
                dlt_log_debug(APP_ID, "Switching to section [%s]", current_section);
            }
            continue;
        }

        // 解析键值对
        parse_key_value(line, current_section, &config->http, &config->ftp);
    }

    fclose(file);
    dlt_log_debug(APP_ID, "Configuration loaded from file: %s", filename);
    return 0; // 成功
}

// 打印配置信息
void config_print(const ServerConfig *config) {
    if (config == NULL) return;
    
    printf("Server Configuration:\n");
    printf("=====================\n");
    
    printf("HTTP Server:\n");
    printf("  IP: %s\n", config->http.ip);
    printf("  Port: %d\n", config->http.port);
    printf("  Root Directory: %s\n", config->http.root_dir);
    printf("  Max Connections: %d\n", config->http.max_connections);
    
    printf("\nFTP Server:\n");
    printf("  IP: %s\n", config->ftp.ip);
    printf("  Port: %d\n", config->ftp.port);
    printf("  Root Directory: %s\n", config->ftp.root_dir);
    printf("  Max Connections: %d\n", config->ftp.max_connections);
    printf("  Data Port Range: %d-%d\n", 
           config->ftp.data_port_min, config->ftp.data_port_max);
}
