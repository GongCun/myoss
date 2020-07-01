#include "oss_api.h"
#include "aos_http_io.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>

#define MAX_LINE 2048
#define COPYINCR (1024*1024*1024) /* 1 GB */

int proxy_flag;
char *bucket_name;
char *object_name;
char *endpoint;
char *access_key_id;
char *access_key_secret;
char *proxy_host;
char *proxy_user;
char *proxy_passwd;
char *proxy_port;
unsigned long long i, total;
unsigned long long fsz = 0, length = 0;

static void sig_int(int signo)
{
    printf("offset = %llu\n", fsz);
    exit(2);
}

static void percentage(int64_t consumed_bytes, int64_t total_bytes)
{
    assert(total_bytes >= consumed_bytes);
    printf("%%%" APR_INT64_T_FMT "\n", consumed_bytes * 100 / total_bytes);
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


int main(int argc, char *argv[])
{
    int c;
    int fd = -1;
    struct stat sbuf;
    unsigned long size = 0; /* maximum to 4GB */
    unsigned long copysz = 0;
    void *object_content;

    opterr = 0;

    while ((c = getopt(argc, argv, "pb:o:f:s:l:")) != -1)
        switch (c) {
        case 'p':
            proxy_flag = 1;
            break;
        case 'b':
            bucket_name = strdup(optarg);
            break;
        case 'o':
            object_name = strdup(optarg);
            break;
        case 'f':
            fd = open(optarg, O_RDONLY);
            if (fd < 0) {
                perror("open file");
                exit(1);
            }
            break;
        case 's':
            size = strtoul(optarg, NULL, 10);
            break;
        case 'l':
            length = strtoull(optarg, NULL, 10);
            fsz += length;
            break;
        default:
            fprintf(stderr, "append -p -b bucket_name -o object_name\n");
            exit(1);
        }

    if (!size)
        size = COPYINCR;

    /* 在程序入口调用aos_http_io_initialize方法来初始化网络、内存等全局资源。*/
    if (aos_http_io_initialize(NULL, 0) != AOSE_OK) {
        exit(1);
    }

    /* 用于内存管理的内存池（pool），等价于apr_pool_t。其实现代码在apr库中。*/
    aos_pool_t *pool;

    /* 重新创建一个内存池，第二个参数是NULL，表示没有继承其它内存池。*/
    aos_pool_create(&pool, NULL);

    /* 创建并初始化options，该参数包括endpoint、access_key_id、acces_key_secret、
     * is_cname、curl等全局配置信息。*/
    oss_request_options_t *oss_client_options;

    /* 在内存池中分配内存给options。*/
    oss_client_options = oss_request_options_create(pool);

    /* 初始化Client的选项oss_client_options。*/
    if (!proxy_flag)
        init_options(oss_client_options);
    else {
        init_proxy_options(oss_client_options);
    }

    /* 初始化参数。*/
    aos_string_t bucket;
    aos_string_t object;
    aos_list_t buffer;
    aos_list_t resp_body;
    uint64_t initcrc = 0;
    unsigned long long position = 0;
    char *next_append_position = NULL;
    aos_buf_t *content = NULL;
    aos_table_t *headers1 = NULL;
    aos_table_t *headers2 = NULL;
    aos_table_t *resp_headers = NULL;
    aos_status_t *resp_status = NULL;
    
    aos_str_set(&bucket, bucket_name);
    aos_str_set(&object, object_name);

    if (fstat(fd, &sbuf) < 0) {
        perror("fstat");
        exit(1);
    }

    printf("file size: %llu\n", (unsigned long long)sbuf.st_size);
    printf("need transfer: %llu\n", sbuf.st_size - fsz);
    total = (unsigned long long)((sbuf.st_size -fsz + size -1) / size);
    printf("split to numbers: %llu\n", total);

    sigset_t newmask, oldmask;
    if (signal(SIGINT, sig_int) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }

    while (fsz < sbuf.st_size) {
        if ((sbuf.st_size - fsz) < size)
            copysz = sbuf.st_size - fsz;
        else
            copysz = size;

        if ((object_content = mmap(0, copysz, PROT_READ, MAP_SHARED,
                                   fd, fsz)) == MAP_FAILED)
        {
            perror("mmap");
            exit(1);
        }

        /* 获取起始追加位置。*/
        if (i == 0) {
            headers1 = aos_table_make(pool, 0);
            resp_status = oss_head_object(oss_client_options,
                                          &bucket,
                                          &object,
                                          headers1,
                                          &resp_headers);

            if (aos_status_is_ok(resp_status)) {
                next_append_position = (char *)(apr_table_get(resp_headers,
                                                              "x-oss-next-append-position"));
                position = strtoull(next_append_position, NULL, 10);
            }
        }

        /* 追加文件。*/
        headers2 = aos_table_make(pool, 0);
        aos_list_init(&buffer);
        content = aos_buf_pack(pool, object_content, copysz);
        aos_list_add_tail(&content->node, &buffer);

        /* 阻塞 SIGINT (Ctrl-C) 信号，完成上传操作原子化。*/
        sigemptyset(&newmask);
        sigaddset(&newmask, SIGINT);
        if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {
            perror("sigprocmask");
            exit(1);
        }

        resp_status = oss_do_append_object_from_buffer(oss_client_options,
                                                       &bucket,
                                                       &object,
                                                       position,
                                                       initcrc,
                                                       &buffer,
                                                       headers2,
                                                       NULL,
                                                       percentage,
                                                       &resp_headers,
                                                       &resp_body);

        if (aos_status_is_ok(resp_status)) {
            printf("append object from buffer succeeded (%llu)\n", i++);
        } else {
            printf("append object from buffer failed\n");
            exit(1);
        }

        next_append_position = (char *)(apr_table_get(resp_headers, "x-oss-next-append-position"));
        position = strtoull(next_append_position, NULL, 10);
        initcrc = aos_atoui64((char*)(apr_table_get(resp_headers, OSS_HASH_CRC64_ECMA)));

        munmap(object_content, copysz);
        fsz += copysz;

        /* 递送 SIGINT 信号（如有）。*/
        if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
            perror("sigprocmask");
            exit(1);
        }


    }

    /* 释放内存池，相当于释放了请求过程中各资源分配的内存。*/
    aos_pool_destroy(pool);
    /* 释放之前分配的全局资源。*/
    aos_http_io_deinitialize();

    free(bucket_name);
    free(object_name);
    free(endpoint);
    free(access_key_id);
    free(access_key_secret);
    free(proxy_host);
    free(proxy_user);
    free(proxy_passwd);
    free(proxy_port);

    return 0;
}
