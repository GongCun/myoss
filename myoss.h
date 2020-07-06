#ifndef __MY_OSS
#define __MY_OSS

#include "oss_api.h"
#include "aos_http_io.h"
#include <sys/stat.h>

#define MAX_LINE 2048

int proxy_flag;
char *endpoint;
char *access_key_id;
char *access_key_secret;
char *proxy_host;
char *proxy_user;
char *proxy_passwd;
char *proxy_port;

void percentage(int64_t consumed_bytes, int64_t total_bytes);
void init_options(oss_request_options_t *options);
void init_proxy_options(oss_request_options_t *options);
void myoss_free(void);
int64_t get_file_size(const char *file_path);

#endif

