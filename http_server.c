#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>

#include "config.h"
#include "server.h"
#include "http_server.h"
#include "utils.h"


#define BUFFER_SIZE 4096
#define MAX_PATH 4096

// 全局变量，保存HTTP服务器socket
static int http_server_fd = -1;



// 发送HTTP响应头
static void send_http_header(int client_fd, int status_code, 
                            const char *content_type, off_t content_length) {
    char header[BUFFER_SIZE];
    const char *status_msg;
    
    switch (status_code) {
        case 200: status_msg = "OK"; break;
        case 403: status_msg = "Forbidden"; break;
        case 404: status_msg = "Not Found"; break;
        case 414: status_msg = "Request-URI Too Long"; break;
        case 500: status_msg = "Internal Server Error"; break;
        default:  status_msg = "Unknown";
    }
    
    snprintf(header, sizeof(header), 
             "HTTP/1.1 %d %s\r\n"
             "Server: MultiProtocolServer\r\n"
             "Content-Type: %s\r\n",
             status_code, status_msg, content_type);
    
    if (content_length >= 0) {
        char length_str[64];
        snprintf(length_str, sizeof(length_str), "Content-Length: %ld\r\n", (long)content_length);
        strncat(header, length_str, sizeof(header) - strlen(header) - 1);
    }
    
    strncat(header, "\r\n", sizeof(header) - strlen(header) - 1);
    ssize_t written = write(client_fd, header, strlen(header));
    if (written <= 0) {
        perror("write");
        // Optionally handle partial write or error
    }
}

// 发送错误页面
static void send_error_page(int client_fd, int status_code) {
    const char *title, *message;
    
    switch (status_code) {
        case 403:
            title = "403 Forbidden";
            message = "You don't have permission to access this resource.";
            break;
        case 404:
            title = "404 Not Found";
            message = "The requested resource was not found on this server.";
            break;
        case 414:
            title = "414 Request-URI Too Long";
            message = "The requested URL is too long for the server to process.";
            break;
        case 500:
            title = "500 Internal Server Error";
            message = "The server encountered an internal error.";
            break;
        default:
            title = "Error";
            message = "An unknown error occurred.";
    }
    
    char html[BUFFER_SIZE];
    snprintf(html, sizeof(html),
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "    <title>%s</title>\n"
             "    <style>\n"
             "        body { font-family: Arial, sans-serif; max-width: 1200px; margin: 0 auto; padding: 20px; }\n"
             "        .header { background-color: #f5f5f5; padding: 10px; border-radius: 5px; margin-bottom: 20px; }\n"
             "        .error { color: #dc3545; text-align: center; margin-top: 50px; }\n"
             "    </style>\n"
             "</head>\n"
             "<body>\n"
             "    <div class=\"header\">\n"
             "        <h1>MultiProtocol Server</h1>\n"
             "    </div>\n"
             "    <div class=\"error\">\n"
             "        <h2>%s</h2>\n"
             "        <p>%s</p>\n"
             "        <p><a href=\"/\">Return to home</a></p>\n"
             "    </div>\n"
             "</body>\n"
             "</html>",
             title, title, message);
    
    send_http_header(client_fd, status_code, "text/html", strlen(html));
    ssize_t written = write(client_fd, html, strlen(html));
    if (written <= 0) {
        perror("write");
        // Optionally handle partial write or error
    }
}

// 发送目录列表页面
static void send_directory_listing(int client_fd, const char *request_path, 
                                  const char *full_path, const HttpServerConfig *http_config) {
    DIR *dir;
    struct dirent *entry;
    char html[BUFFER_SIZE * 4];
    int html_len = 0;
    
    // 开始构建HTML
    html_len += snprintf(html + html_len, sizeof(html) - html_len,
                        "<!DOCTYPE html>\n"
                        "<html>\n"
                        "<head>\n"
                        "    <title>Index of %s</title>\n"
                        "    <style>\n"
                        "        body { font-family: Arial, sans-serif; max-width: 1200px; margin: 0 auto; padding: 20px; }\n"
                        "        .header { background-color: #f5f5f5; padding: 10px; border-radius: 5px; margin-bottom: 20px; }\n"
                        "        table { width: 100%%; border-collapse: collapse; }\n"
                        "        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }\n"
                        "        th { background-color: #f8f9fa; }\n"
                        "        tr:hover { background-color: #f5f5f5; }\n"
                        "        a { color: #007bff; text-decoration: none; }\n"
                        "        a:hover { text-decoration: underline; }\n"
                        "        .dir { font-weight: bold; }\n"
                        "        .size { text-align: right; }\n"
                        "    </style>\n"
                        "</head>\n"
                        "<body>\n"
                        "    <div class=\"header\">\n"
                        "        <h1>Index of %s</h1>\n"
                        "    </div>\n"
                        "    <table>\n"
                        "        <tr>\n"
                        "            <th>Name</th>\n"
                        "            <th>Last modified</th>\n"
                        "            <th class=\"size\">Size</th>\n"
                        "        </tr>\n",
                        request_path, request_path);
    
    // 尝试打开目录
    if ((dir = opendir(full_path)) == NULL) {
        send_error_page(client_fd, 403);
        return;
    }
    
    // 添加上级目录链接（如果不是根目录）
    if (strcmp(request_path, "/") != 0) {
        char parent_path[MAX_PATH];
        const char *last_slash = strrchr(request_path, '/');
        
        if (last_slash == request_path) {
            strcpy(parent_path, "/");
        } else {
            size_t parent_len = last_slash - request_path;
            if (parent_len >= MAX_PATH) {
                goto skip_parent;  // 路径过长，跳过上级目录处理
            }
            strncpy(parent_path, request_path, parent_len);
            parent_path[parent_len] = '\0';
            if (parent_path[0] == '\0') strcpy(parent_path, "/");
        }
        
        html_len += snprintf(html + html_len, sizeof(html) - html_len,
                            "        <tr>\n"
                            "            <td><a href=\"%s\" class=\"dir\">../</a></td>\n"
                            "            <td></td>\n"
                            "            <td class=\"size\"></td>\n"
                            "        </tr>\n",
                            parent_path);
    }
    skip_parent:  // 用于跳过上级目录处理的标签
    
    // 列出目录中的所有条目
    while ((entry = readdir(dir)) != NULL) {
        // 跳过.和..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // 安全构建条目路径
        char entry_path[MAX_PATH];
        if (!safe_path_join(entry_path, sizeof(entry_path), full_path, entry->d_name, "/")) {
            fprintf(stderr, "Path too long: %s/%s\n", full_path, entry->d_name);
            continue;  // 路径过长，跳过该条目
        }
        
        struct stat st;
        if (stat(entry_path, &st) == -1)
            continue;
        
        // 安全构建URL路径
        char url_path[MAX_PATH];
        if (strcmp(request_path, "/") == 0) {
            if (!safe_path_join(url_path, sizeof(url_path), "", entry->d_name, "/")) {
                fprintf(stderr, "URL too long: /%s\n", entry->d_name);
                continue;
            }
        } else {
            if (!safe_path_join(url_path, sizeof(url_path), request_path, entry->d_name, "/")) {
                fprintf(stderr, "URL too long: %s/%s\n", request_path, entry->d_name);
                continue;
            }
        }
        
        // 如果是目录，确保路径以斜杠结尾
        if (S_ISDIR(st.st_mode) && url_path[strlen(url_path)-1] != '/') {
            if (strlen(url_path) + 1 < sizeof(url_path)) {
                strcat(url_path, "/");
            } else {
                fprintf(stderr, "URL too long: %s/\n", url_path);
                continue;
            }
        }
        
        // 格式化最后修改时间
        char time_str[64];
        struct tm *tm_info = localtime(&st.st_mtime);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
        
        // 获取文件大小
        const char *size_str = "-";
        if (!S_ISDIR(st.st_mode)) {
            size_str = get_file_size_str(st.st_size);
        }
        
        // 添加到HTML
        html_len += snprintf(html + html_len, sizeof(html) - html_len,
                            "        <tr>\n"
                            "            <td><a href=\"%s\" class=\"%s\">%s%s</a></td>\n"
                            "            <td>%s</td>\n"
                            "            <td class=\"size\">%s</td>\n"
                            "        </tr>\n",
                            url_path,
                            S_ISDIR(st.st_mode) ? "dir" : "file",
                            entry->d_name,
                            S_ISDIR(st.st_mode) ? "/" : "",
                            time_str,
                            size_str);
    }
    
    closedir(dir);
    
    // 完成HTML
    html_len += snprintf(html + html_len, sizeof(html) - html_len,
                        "    </table>\n"
                        "    <div style=\"margin-top: 20px; color: #666;\">\n"
                        "        MultiProtocol Server - HTTP on port %d\n"
                        "    </div>\n"
                        "</body>\n"
                        "</html>",
                        http_config->port);
    
    // 发送响应
    send_http_header(client_fd, 200, "text/html", html_len);
    ssize_t written = write(client_fd, html, html_len);
    if (written != html_len) {
    perror("write");
    // Optionally handle partial write or error
    }
}

// 发送文件内容
static void send_file(int client_fd, const char *full_path) {
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        send_error_page(client_fd, 403);
        return;
    }
    
    // 获取文件信息
    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        send_error_page(client_fd, 500);
        return;
    }
    
    // 确定MIME类型
    const char *mime_type = "application/octet-stream";
    const char *ext = strrchr(full_path, '.');
    if (ext) {
        if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
            mime_type = "text/html";
        else if (strcmp(ext, ".txt") == 0)
            mime_type = "text/plain";
        else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
            mime_type = "image/jpeg";
        else if (strcmp(ext, ".png") == 0)
            mime_type = "image/png";
        else if (strcmp(ext, ".gif") == 0)
            mime_type = "image/gif";
        else if (strcmp(ext, ".css") == 0)
            mime_type = "text/css";
        else if (strcmp(ext, ".js") == 0)
            mime_type = "application/javascript";
        else if (strcmp(ext, ".pdf") == 0)
            mime_type = "application/pdf";
    }
    
    // 发送HTTP头
    send_http_header(client_fd, 200, mime_type, st.st_size);
    
    // 发送文件内容
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(client_fd, buffer, bytes_read) != bytes_read) {
            perror("write");
            break;
        }
    }
    
    close(fd);
}

// 处理客户端请求
static void handle_client(int client_fd, const HttpServerConfig *http_config) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    // 解析HTTP请求行
    char method[16], path[MAX_PATH], version[16];
    if (sscanf(buffer, "%s %s %s", method, path, version) != 3) {
        send_error_page(client_fd, 500);
        close(client_fd);
        return;
    }
    
    // 只支持GET方法
    if (strcmp(method, "GET") != 0) {
        send_error_page(client_fd, 403);
        close(client_fd);
        return;
    }
    
    // 构建完整文件系统路径
    char full_path[MAX_PATH];
    if (strcmp(path, "/") == 0) {
        strncpy(full_path, http_config->root_dir, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    } else {
        // URL解码
        char decoded_path[MAX_PATH];
        long unsigned int i = 0, j = 0;
        while (path[i] && j < sizeof(decoded_path) - 1) {
            if (path[i] == '%' && isxdigit(path[i+1]) && isxdigit(path[i+2])) {
                // 简单的URL解码
                char hex[3] = {path[i+1], path[i+2], '\0'};
                decoded_path[j++] = strtol(hex, NULL, 16);
                i += 3;
            } else {
                decoded_path[j++] = path[i++];
            }
        }
        decoded_path[j] = '\0';
        // 安全拼接根目录和解码后的路径
        if (!safe_path_join(full_path, sizeof(full_path), http_config->root_dir, decoded_path, "")) {
            send_error_page(client_fd, 414); // 请求URL过长
            close(client_fd);
            return;
        }
    }
    // 检查路径安全性
    if (!is_path_safe(full_path, http_config->root_dir)) {
        send_error_page(client_fd, 403);
        close(client_fd);
        return;
    }
    
    // 检查文件/目录是否存在
    struct stat st;
    if (stat(full_path, &st) == -1) {
        send_error_page(client_fd, 404);
        close(client_fd);
        return;
    }
    
    // 如果是目录，发送目录列表
    if (S_ISDIR(st.st_mode)) {
        send_directory_listing(client_fd, path, full_path, http_config);
    } 
    // 如果是文件，发送文件内容
    else if (S_ISREG(st.st_mode)) {
        send_file(client_fd, full_path);
    } 
    // 其他类型（如设备文件）禁止访问
    else {
        send_error_page(client_fd, 403);
    }
    
    close(client_fd);
}

// HTTP服务器主函数
int http_server_main(const HttpServerConfig *http_config) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // 创建socket
    if ((http_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("HTTP socket creation failed");
        return -1;
    }
    
    // 设置socket选项，允许端口重用
    int opt = 1;
    if (setsockopt(http_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("HTTP setsockopt failed");
        close(http_server_fd);
        return -1;
    }
    
    // 绑定地址和端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(http_config->ip);
    server_addr.sin_port = htons(http_config->port);
    
    if (bind(http_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("HTTP bind failed");
        close(http_server_fd);
        return -1;
    }
    
    // 监听连接
    if (listen(http_server_fd, 10) == -1) {
        perror("HTTP listen failed");
        close(http_server_fd);
        return -1;
    }
    
    printf("HTTP server running on port %d, root directory: %s\n", http_config->port, http_config->root_dir);
    // 创建根目录（如果不存在）
    mkdir(http_config->root_dir, 0755);
    
    // 主循环，接受并处理连接
    while (server_running) {
        // 设置非阻塞模式，以便能响应退出信号
        int flags = fcntl(http_server_fd, F_GETFL, 0);
        fcntl(http_server_fd, F_SETFL, flags | O_NONBLOCK);
        
        int client_fd = accept(http_server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        // 恢复阻塞模式
        fcntl(http_server_fd, F_SETFL, flags);
        
        if (client_fd == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("HTTP accept failed");
            }
            // 短暂休眠，减少CPU占用
            usleep(10000);
            continue;
        }
        
        printf("HTTP: Received connection from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        // 处理客户端请求
    handle_client(client_fd, http_config);
    }
    
    // 关闭服务器socket
    if (http_server_fd != -1) {
        close(http_server_fd);
        http_server_fd = -1;
    }
    
    printf("HTTP server stopped\n");
    return 0;
}

// HTTP服务器线程函数
void *run_http_server(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    if (data == NULL || data->config == NULL) {
        fprintf(stderr, "HTTP server thread received invalid data\n");
        return NULL;
    }
    http_server_main(&data->config->http);
    return NULL;
}
