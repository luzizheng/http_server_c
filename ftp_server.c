
#include "utils.h"
#include "ftp_server.h"
#include "logMgr.h"
#include "config.h"

#define APP_ID "SRV"


// 客户端数据接口
typedef struct {
    int control_sock;
    int data_sock;
    struct sockaddr_in client_addr;
    pthread_t thread_id;
    int is_active;
    ServerConfig *config;
} client_data_t;

extern volatile bool server_running;

// 全局变量
client_data_t clients[SERVER_DEFAULT_FTP_MAX_CONN];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


// 发送响应到客户端
void send_response(int sock, int code, const char *message) {
    char buffer[256];
    int len = snprintf(buffer, sizeof(buffer), "%d %s\r\n", code, message);
    if (len > 0) {
        send(sock, buffer, len, 0);
    }else{
        dlt_log_error(APP_ID, "send_response: snprintf error");
    }
    print_raw_data("Sent response", buffer, len);
}

int is_path_valid(const char *path, const char *root_dir) {
    char real_path[512];
    char real_root[512];
    if (realpath(path, real_path) == NULL) {
        return 0;
    }
    if (realpath(root_dir, real_root) == NULL) {
        return 0;
    }
    return strncmp(real_path, real_root, strlen(real_root)) == 0;
}

// 处理LIST命令
void handle_list(const ServerConfig *srv_cfg, int control_sock, int data_sock, const char *path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char buffer[1024];
    time_t rawtime;
    struct tm *timeinfo;

    if(!is_path_valid(path, srv_cfg->ftp.root_dir)) {
        send_response(control_sock, 550, "Requested action not taken. File unavailable.");
        return;
    }

    dir = opendir(path);
    if (dir == NULL) {
        send_response(control_sock, 550, "Failed to open directory.");
        return;
    }

    send_response(control_sock, 150, "Here comes the directory listing.");

    while ((entry = readdir(dir)) != NULL)
    {
        // 跳过当前目录和父目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &file_stat) == -1) {
            dlt_log_error(APP_ID, "Failed to get file status for %s: %s", full_path, strerror(errno));
            continue;
        }

        // 文件权限
        char perms[11];
        snprintf(perms, sizeof(perms), "%c%c%c%c%c%c%c%c%c%c",
                 S_ISDIR(file_stat.st_mode) ? 'd' : '-',
                 (file_stat.st_mode & S_IRUSR) ? 'r' : '-',
                 (file_stat.st_mode & S_IWUSR) ? 'w' : '-',
                 (file_stat.st_mode & S_IXUSR) ? 'x' : '-',
                 (file_stat.st_mode & S_IRGRP) ? 'r' : '-',
                 (file_stat.st_mode & S_IWGRP) ? 'w' : '-',
                 (file_stat.st_mode & S_IXGRP) ? 'x' : '-',
                 (file_stat.st_mode & S_IROTH) ? 'r' : '-',
                 (file_stat.st_mode & S_IWOTH) ? 'w' : '-',
                 (file_stat.st_mode & S_IXOTH) ? 'x' : '-');
        
        // 文件时间
        rawtime = file_stat.st_mtime;
        timeinfo = localtime(&rawtime);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%b %d %H:%M", timeinfo);

        // 格式化目录列表行
        snprintf(buffer, sizeof(buffer), "%s %3ld %-8d %-8d %-8d %-8ld %s %s\r\n",
                 perms,
                 (long)file_stat.st_nlink,
                 (long)file_stat.st_uid,
                 (long)file_stat.st_gid,
                 (long)file_stat.st_size,
                 time_str,
                 entry->d_name);
        send(data_sock, buffer, strlen(buffer), 0);
        print_raw_data("Sent LIST entry", buffer, strlen(buffer));
    }
    closedir(dir);
    send_response(control_sock, 226, "Directory send OK.");
}

// 处理RETR命令(下载文件)
void handle_retr(const ServerConfig *srv_cfg, int control_sock, int data_sock, const char *path)
{
    int file_fd;
    off_t offset = 0;
    struct stat file_stat;

    if(!is_path_valid(path, srv_cfg->ftp.root_dir)) {
        send_response(control_sock, 550, "Requested action not taken. File unavailable.");
        return;
    }

    file_fd = open(path, O_RDONLY);
    if (file_fd < 0) {
        send_response(control_sock, 550, "Failed to open file.");
        return;
    }

    if (fstat(file_fd, &file_stat) < 0) {
        close(file_fd);
        send_response(control_sock, 550, "Failed to get file status.");
        return;
    }

    send_response(control_sock, 150, "Opening binary mode data connection for file transfer.");

    // 使用sendfile发送文件
    ssize_t sent_bytes = sendfile(data_sock, file_fd, &offset, file_stat.st_size);
    if (sent_bytes < 0) {
        dlt_log_error(APP_ID, "Failed to send file: %s", strerror(errno));
        send_response(control_sock, 426, "Connection closed; transfer aborted.");
    } else {
        dlt_log_debug(APP_ID, "Sent %zd bytes for file %s", sent_bytes, path);
        send_response(control_sock, 226, "Transfer complete.");
    }

    close(file_fd);
}

// 处理客户端命令
void handle_client_commands(const ServerConfig *srv_cfg, int control_sock, struct sockaddr_in *client_addr)
{
    char buffer[512];
    char cmd[16], arg[256];
    int data_sock = -1;
    int logged_in = 0;
    send_response(control_sock, 220, "Welcome to Simple FTP Server");

    while (1) {
        ssize_t n = recv(control_sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            if (n < 0) {
                dlt_log_error(APP_ID, "recv error: %s", strerror(errno));
            } else {
                dlt_log_debug(APP_ID, "Client disconnected.");
            }
            break;
        }
        buffer[n] = '\0';
        print_raw_data("Received command", buffer, n);

        // 解析命令和参数
        sscanf(buffer, "%15s %255[^\" ]", cmd, arg); // fallback: parse until space or end
        for (int i = 0; cmd[i]; i++) cmd[i] = toupper(cmd[i]);

        if (strcmp(cmd, "USER") == 0) {
            send_response(control_sock, 331, "User name okay, need password.");
        } else if (strcmp(cmd, "PASS") == 0) {
            logged_in = 1;
            send_response(control_sock, 230, "User logged in, proceed.");
        } else if (strcmp(cmd, "QUIT") == 0) {
            send_response(control_sock, 221, "Goodbye.");
            break;
        } else if (strcmp(cmd, "SYST") == 0) {
            send_response(control_sock, 215, "UNIX Type: L8");
        } else if (strcmp(cmd, "PWD") == 0) {
            send_response(control_sock, 257, srv_cfg->ftp.root_dir);
        } else if (strcmp(cmd, "TYPE") == 0) {
            send_response(control_sock, 200, "Type set to I.");
        } else if (strcmp(cmd, "PASV") == 0) {
            // 启动被动模式，监听数据端口
            int pasv_sock;
            struct sockaddr_in pasv_addr;
            socklen_t addrlen = sizeof(pasv_addr);
            pasv_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (pasv_sock < 0) {
                send_response(control_sock, 425, "Can't open passive connection.");
                continue;
            }
            memset(&pasv_addr, 0, sizeof(pasv_addr));
            pasv_addr.sin_family = AF_INET;
            pasv_addr.sin_addr.s_addr = inet_addr(srv_cfg->ftp.ip);
            pasv_addr.sin_port = htons(srv_cfg->ftp.data_port_min);
            int opt = 1;
            setsockopt(pasv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            if (bind(pasv_sock, (struct sockaddr*)&pasv_addr, sizeof(pasv_addr)) < 0) {
                close(pasv_sock);
                send_response(control_sock, 425, "Can't bind passive port.");
                continue;
            }
            if (listen(pasv_sock, 1) < 0) {
                close(pasv_sock);
                send_response(control_sock, 425, "Can't listen on passive port.");
                continue;
            }
            getsockname(pasv_sock, (struct sockaddr*)&pasv_addr, &addrlen);
            unsigned int p = ntohs(pasv_addr.sin_port);
            unsigned int ip = ntohl(pasv_addr.sin_addr.s_addr);
            char pasv_msg[128];
            snprintf(pasv_msg, sizeof(pasv_msg),
                "Entering Passive Mode (%u,%u,%u,%u,%u,%u).",
                (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
                (p >> 8) & 0xFF, p & 0xFF);
            send_response(control_sock, 227, pasv_msg);
            // 等待数据连接
            struct sockaddr_in data_client;
            socklen_t dlen = sizeof(data_client);
            data_sock = accept(pasv_sock, (struct sockaddr*)&data_client, &dlen);
            close(pasv_sock);
            if (data_sock < 0) {
                send_response(control_sock, 425, "Failed to accept data connection.");
                continue;
            }
        } else if (strcmp(cmd, "LIST") == 0) {
            if (data_sock < 0) {
                send_response(control_sock, 425, "Use PASV first.");
                continue;
            }
            handle_list(srv_cfg, control_sock, data_sock, srv_cfg->ftp.root_dir);
            close(data_sock);
            data_sock = -1;
        } else if (strcmp(cmd, "RETR") == 0) {
            if (data_sock < 0) {
                send_response(control_sock, 425, "Use PASV first.");
                continue;
            }
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", srv_cfg->ftp.root_dir, arg);
            handle_retr(srv_cfg, control_sock, data_sock, file_path);
            close(data_sock);
            data_sock = -1;
        } else {
            send_response(control_sock, 502, "Command not implemented.");
        }
    }
    // Ensure data_sock is closed if still open (client exited abnormally)
    if (data_sock >= 0) {
        close(data_sock);
        data_sock = -1;
        dlt_log_debug(APP_ID, "Closed lingering data_sock after client exit.");
    }
}

// 客户端处理线程
void *client_thread(void *arg)
{
    client_data_t *client = (client_data_t *)arg;
    dlt_log_debug(APP_ID, "Client thread started.");
    handle_client_commands(client->config, client->control_sock, &client->client_addr);
    close(client->control_sock);
    pthread_mutex_lock(&clients_mutex);
    client->is_active = 0;
    pthread_mutex_unlock(&clients_mutex);
    dlt_log_debug(APP_ID, "Client thread exiting.");
    return NULL;
}

// 初始化服务器
int init_server(const ServerConfig *srv_cfg)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        dlt_log_error(APP_ID, "Failed to create socket: %s", strerror(errno));
        return -1;
    }
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(srv_cfg->ftp.ip);
    serv_addr.sin_port = htons(srv_cfg->ftp.port);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        dlt_log_error(APP_ID, "Failed to bind socket: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    if (listen(sockfd, srv_cfg->ftp.max_connections) < 0) {
        dlt_log_error(APP_ID, "Failed to listen: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    dlt_log_debug(APP_ID, "FTP server listening on %s:%d", srv_cfg->ftp.ip, srv_cfg->ftp.port);
    return sockfd;
}

// ftp服务器主函数入口
int ftp_server_main(void *arg)
{
    const ServerConfig *srv_cfg = (const ServerConfig *)arg;
    int server_sock = init_server(srv_cfg);
    if (server_sock < 0) {
        dlt_log_error(APP_ID, "FTP server failed to start.");
        return -1;
    }
    dlt_log_debug(APP_ID, "FTP server main loop starting.");
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addrlen);
        if (client_sock < 0) {
            if (errno == EINTR) continue;
            dlt_log_error(APP_ID, "Accept failed: %s", strerror(errno));
            continue;
        }
        pthread_mutex_lock(&clients_mutex);
        int slot = -1;
        for (int i = 0; i < srv_cfg->ftp.max_connections; i++) {
            if (!clients[i].is_active) {
                slot = i;
                break;
            }
        }
        if (slot == -1) {
            pthread_mutex_unlock(&clients_mutex);
            send_response(client_sock, 421, "Too many connections.");
            close(client_sock);
            continue;
        }
        clients[slot].control_sock = client_sock;
        clients[slot].client_addr = client_addr;
        clients[slot].is_active = 1;
        clients[slot].config = (ServerConfig *)srv_cfg;
        pthread_create(&clients[slot].thread_id, NULL, client_thread, &clients[slot]);
        pthread_mutex_unlock(&clients_mutex);
    }
    close(server_sock);
    dlt_log_debug(APP_ID, "FTP server main loop exiting.");
    return 0;
}





void *run_ftp_server(void *arg)
{
    ThreadData *data = (ThreadData *)arg;
    if (data == NULL || data->config == NULL) {
        fprintf(stderr, "HTTP server thread received invalid data\n");
        return NULL;
    }
    dlt_log_debug(APP_ID, "FTP server thread started.");
    ftp_server_main(&data->config);
    dlt_log_debug(APP_ID, "FTP server thread exiting.");
    return NULL;
}