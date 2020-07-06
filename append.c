#include "myoss.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>

#define COPYINCR (1024*1024*1024) /* 1 GB */
#define HELP_STR "append [-p] -b bucket_name -o object_name -f local_file -s split_size -l offset\n"

unsigned long long i, total;
unsigned long long fsz = 0, length = 0;

static void sig_int(int signo)
{
    printf("offset = %llu\n", fsz);
    exit(2);
}

int main(int argc, char *argv[])
{
    int c;
    int fd = -1;
    struct stat sbuf;
    unsigned long size = 0; /* maximum to 4GB */
    unsigned long copysz = 0;
    void *object_content;
    char *object_name;
    char *bucket_name;

    opterr = 0;

    while ((c = getopt(argc, argv, "pb:o:f:s:l:")) != EOF)
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
            fprintf(stderr, HELP_STR);
            exit(1);
        }

    if (optind < argc) {
        fprintf(stderr, HELP_STR);
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
    double begin_millisecond = 0;
    double end_millisecond = 0;
    double cost_millisecond = 0;
    struct timespec ts;
    
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
                next_append_position = (char *)(apr_table_get(resp_headers, "x-oss-next-append-position"));
                position = strtoull(next_append_position, NULL, 10);
                initcrc = aos_atoui64((char*)(apr_table_get(resp_headers, OSS_HASH_CRC64_ECMA)));
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

        if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
            perror("clock_gettime");
            exit(1);
        }
        begin_millisecond = ts.tv_sec * 1000 + ((double) ts.tv_nsec) / 1000 / 1000;
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

        if (!aos_status_is_ok(resp_status)) {
            printf("append object from buffer failed\n");
            exit(1);
        }

        if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
            perror("clock_gettime");
            exit(1);
        }
        end_millisecond = ts.tv_sec * 1000 + ((double) ts.tv_nsec) / 1000 / 1000;
        cost_millisecond = end_millisecond - begin_millisecond;

        printf(" append object from buffer succeeded (%llu), speed %.2f(kb/s)\n",
               i++, (((double)copysz) / 1024) / (cost_millisecond / 1000));

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
    myoss_free();

    return 0;
}
