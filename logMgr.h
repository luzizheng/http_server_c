#ifndef LOGMGR_H
#define LOGMGR_H

extern int dlt_init_client(const char *app_id);
extern int dlt_free_client(const char *app_id);


extern int dlt_log_fatal(const char *app_id, const char *format, ...);
extern int dlt_log_error(const char *app_id, const char *format, ...);
extern int dlt_log_warning(const char *app_id, const char *format, ...);
extern int dlt_log_debug(const char *app_id, const char *format, ...); 
extern int dlt_log_info(const char *app_id, const char *format, ...);
extern int dlt_log_verbose(const char *app_id, const char *format, ...);

#endif // LOGMGR_H