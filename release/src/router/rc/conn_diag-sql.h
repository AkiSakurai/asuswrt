#include <sqlite3.h>


#define RTCONFIG_UPLOADER

#define DIAG_TAB_NAME "conn_diag"
#define MAX_DB_SIZE 4194304 // 4MB
#define MAX_DATA 1024

#define SYS_DIR         "/jffs/.sys"
#define DIAG_DB_DIR     SYS_DIR"/diag_db"
#ifdef RTCONFIG_UPLOADER
#define DIAG_CLOUD_DIR  "/tmp/diag_db_cloud"
#define DIAG_CLOUD_UPLOAD  DIAG_CLOUD_DIR"/upload"
#define DIAG_CLOUD_DOWNLOAD  DIAG_CLOUD_DIR"/download"
#endif

enum {
	INIT_DB_NO=0,
	INIT_DB_YES,
	INIT_DB_CLOUD,
	INIT_DB_MAX
};


extern unsigned long get_ts_from_db_name(char *str);
extern int save_data_in_sql(const char *event, char *raw);
extern int specific_data_on_day(unsigned long specific_ts, const char *where, int *row_count, int *field_count, char ***raw);
extern int get_sql_on_day(unsigned long specific_ts, const char *event, const char *node_ip, const char *node_mac,
		int *row_count, int *field_count, char ***raw);
extern int get_json_on_day(unsigned long specific_ts, const char *event, const char *node_ip, const char *node_mac,
		int *row_count, int *field_count, char ***raw);
extern int merge_data_in_sql(const char *dst_file, const char *src_file);
#ifdef RTCONFIG_UPLOADER
extern int run_upload_file_at_ts(unsigned long ts, unsigned long ts2);
extern int run_upload_file_by_name(const char *uploaded_file);
extern int run_download_file_at_ts(unsigned long ts, unsigned long ts2);
extern int run_download_file_by_name(const char *downloaded_file);
#endif
