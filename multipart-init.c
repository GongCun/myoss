#include "myoss.h"

char *bucket_name;
char *object_name;

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "multipart-init <bucket> <object>\n");
        exit(1);
    }
    bucket_name = argv[1];
    object_name = argv[2];

    /* 在程序入口调用aos_http_io_initialize方法来初始化网络、内存等全局资源。*/
    if (aos_http_io_initialize(NULL, 0) != AOSE_OK) {
        exit(1);
    }
    /* 用于内存管理的内存池（pool），等价于apr_pool_t。其实现代码在apr库中。*/
    aos_pool_t *pool;

    /* 重新创建一个内存池，第二个参数是NULL，表示没有继承其它内存池。*/
    aos_pool_create(&pool, NULL);

    /* 创建并初始化options，该参数包括endpoint、access_key_id、acces_key_secret、is_cname、curl等全局配置信息。*/
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
    aos_string_t upload_id;
    aos_table_t *headers = NULL;
    aos_table_t *resp_headers = NULL;
    aos_status_t *resp_status = NULL;

    aos_str_set(&bucket, bucket_name);
    aos_str_set(&object, object_name);
    aos_str_null(&upload_id);
    headers = aos_table_make(pool, 1);

    /* 初始化分片上传，获取一个上传ID(upload_id)。*/
    resp_status = oss_init_multipart_upload(oss_client_options, &bucket, &object, &upload_id, headers, &resp_headers);

    /* 判断分片上传初始化是否成功。*/
    if (aos_status_is_ok(resp_status)) {
        printf("Init multipart upload succeeded, upload_id: %.*s\n",
               upload_id.len, upload_id.data);
    } else {
        printf("Init multipart upload failed, upload_id: %.*s\n",
               upload_id.len, upload_id.data);
    }


    /* 释放内存池，相当于释放了请求过程中各资源分配的内存。*/
    aos_pool_destroy(pool);
    /* 释放之前分配的全局资源。*/
    aos_http_io_deinitialize();

    myoss_free();

    return 0;
}
