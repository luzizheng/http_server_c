
#include "ftp_server.h"
#include <stdio.h>
#include "logMgr.h"

#define APP_ID "SRV"

void *run_ftp_server(void *arg)
{
    dlt_log_debug(APP_ID, "FTP server thread started.");
    // FTP服务器的实现代码应放在这里
    dlt_log_debug(APP_ID, "FTP server thread exiting.");
    return NULL;
}