#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <libgen.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <stdbool.h>

int safe_path_join(char *dest, size_t dest_size, const char *path1, const char *path2, const char *separator);
int is_path_safe(const char *path, const char *root_dir);
const char* get_file_size_str(off_t size);
void print_raw_data(const char *prefix, const char *data, size_t len);
char *get_local_ip();
long get_file_size(const char *filename);
#endif // UTILS_H