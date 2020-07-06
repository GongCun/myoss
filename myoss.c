#include "myoss.h"
#include <unistd.h>

void percentage(int64_t consumed_bytes, int64_t total_bytes)
{
    if (isatty(1)) {
        FILE *fpin = NULL;
        char line[MAX_LINE];
        char cmd[MAX_LINE];

        fpin = popen("/usr/bin/tput lines", "r");
        if (fpin == NULL) {
            perror("popen");
            exit(1);
        }
        fgets(line, MAX_LINE, fpin);
        pclose(fpin);
        snprintf(cmd, MAX_LINE, "/usr/bin/tput cup %d 0", atoi(line)-1);
        system(cmd);
    }
    assert(total_bytes >= consumed_bytes);
    printf("%%%" APR_INT64_T_FMT " ", consumed_bytes * 100 / total_bytes);
    fflush(stdout);
}

void init_options(oss_request_options_t *options)
{
    endpoint = strdup(getenv("ENDPOINT"));
    access_key_id = strdup(getenv("ACCESS_KEY_ID"));
    access_key_secret = strdup(getenv("ACCESS_KEY_SECRET"));

    options->config = oss_config_create(options->pool);

    /* 用char*类型的字符串初始化aos_string_t类型。*/
    aos_str_set(&options->config->endpoint, endpoint);
    aos_str_set(&options->config->access_key_id, access_key_id);
    aos_str_set(&options->config->access_key_secret, access_key_secret);

    /* 是否使用了CNAME。0表示不使用。*/
    options->config->is_cname = 0;

    /* 用于设置网络相关参数，比如超时时间等。*/
    options->ctl = aos_http_controller_create(options->pool, 0);
}

void init_proxy_options(oss_request_options_t *options)
{

    endpoint = strdup(getenv("ENDPOINT"));
    access_key_id = strdup(getenv("ACCESS_KEY_ID"));
    access_key_secret = strdup(getenv("ACCESS_KEY_SECRET"));
    proxy_host = strdup(getenv("PROXY_HOST"));
    proxy_user = strdup(getenv("PROXY_USER"));
    proxy_passwd = strdup(getenv("PROXY_PASSWD"));
    proxy_port = strdup(getenv("PROXY_PORT"));

    options->config = oss_config_create(options->pool);

    /* 用char*类型的字符串初始化aos_string_t类型。*/
    aos_str_set(&options->config->endpoint, endpoint);
    aos_str_set(&options->config->access_key_id, access_key_id);
    aos_str_set(&options->config->access_key_secret, access_key_secret);
    aos_str_set(&options->config->proxy_host, proxy_host);
    aos_str_set(&options->config->proxy_user, proxy_user);
    aos_str_set(&options->config->proxy_passwd, proxy_passwd);
    options->config->proxy_port = atoi(proxy_port);

    /* 是否使用了CNAME。0表示不使用。*/
    options->config->is_cname = 0;

    /* 用于设置网络相关参数，比如超时时间等。*/
    options->ctl = aos_http_controller_create(options->pool, 0);
    options->ctl->options = aos_http_request_options_create(options->pool);
    oss_config_resolve(options->pool, options->config, options->ctl);
    options->ctl->options->verify_ssl = AOS_FALSE;
}

void myoss_free(void)
{
    free(endpoint);
    free(access_key_id);
    free(access_key_secret);
    free(proxy_host);
    free(proxy_user);
    free(proxy_passwd);
    free(proxy_port);
}
