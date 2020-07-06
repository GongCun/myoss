#include "myoss.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "resume <bucket> <local-file> <object>\n");
        exit(1);
    }

    /* 在程序入口调用aos_http_io_initialize方法来初始化网络、内存等全局资源。*/
    if (aos_http_io_initialize(NULL, 0) != AOSE_OK) {
        exit(1);
    }

    /* 用于内存管理的内存池（pool），等价于apr_pool_t。其实现代码在apr库中。*/
    aos_pool_t *pool;

    /* 重新创建一个新的内存池，第二个参数是NULL，表示没有继承其它内存池。*/
    aos_pool_create(&pool, NULL);

    /* 创建并初始化options，该参数包括endpoint、access_key_id、acces_key_secret、
     * is_cname、curl等全局配置信息。*/
    oss_request_options_t *oss_client_options;

    /* 在内存池中分配内存给options。*/
    oss_client_options = oss_request_options_create(pool);

    /* 初始化Client的选项oss_client_options。*/
    if (getenv("PROXY_HOST"))
        init_proxy_options(oss_client_options);
    else
        init_options(oss_client_options);

    /* 初始化参数。*/
    aos_string_t bucket;
    aos_string_t object;
    aos_string_t file;
    aos_list_t resp_body;
    aos_table_t *headers = aos_table_make(pool, 0);
    aos_table_t *resp_headers = NULL;
    aos_status_t *resp_status = NULL;
    oss_resumable_clt_params_t *clt_params;

    aos_str_set(&bucket, argv[1]);
    aos_str_set(&file, argv[2]);
    aos_str_set(&object, argv[3]);
    aos_list_init(&resp_body);

    /* 断点续传。*/
    clt_params = oss_create_resumable_clt_params_content(pool, 1024 * 1024, 1, AOS_TRUE, NULL);
    resp_status = oss_resumable_upload_file(oss_client_options,
                                            &bucket,
                                            &object,
                                            &file,
                                            headers,
                                            NULL,
                                            clt_params,
                                            percentage,
                                            &resp_headers,
                                            &resp_body);

    if (aos_status_is_ok(resp_status)) {
        printf("resumable upload succeeded\n");
    } else {
        printf("resumable upload failed\n");
    }

    /* 释放内存池，相当于释放了请求过程中各资源分配的内存。*/
    aos_pool_destroy(pool);
    /* 释放之前分配的全局资源。*/
    aos_http_io_deinitialize();

    /* 释放为环境变量分配的内存。*/
    myoss_free();

    return 0;
}
