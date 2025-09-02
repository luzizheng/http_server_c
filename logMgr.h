#ifndef USE_DLT_LIB
#ifndef LOGMGR_H
#define LOGMGR_H

int dlt_init_client(const char *app_id);
int dlt_free_client(const char *app_id);


int dlt_log_fatal(const char *app_id, const char *format, ...);
int dlt_log_error(const char *app_id, const char *format, ...);
int dlt_log_warn(const char *app_id, const char *format, ...);
int dlt_log_debug(const char *app_id, const char *format, ...); 
int dlt_log_info(const char *app_id, const char *format, ...);
int dlt_log_verbose(const char *app_id, const char *format, ...);

#endif // LOGMGR_H
#endif // USE_DLT_LIB