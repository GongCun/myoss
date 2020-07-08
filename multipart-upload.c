#include "myoss.h"

char *bucket_name;
char *object_name;
char *local_filename;
char *upload_id_str;
char *part_num_str;

int main(int argc, char *argv[])
{
    if (argc != 6) {
        fprintf(stderr, "multipart-upload <bucket> <local-file> <object> <upload-id> <part-number>\n");
        exit(1);
    }
    bucket_name = argv[1];
    local_filename = argv[2];
    object_name = argv[3];
    upload_id_str = argv[4];
    part_num_str = argv[5];

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
    oss_upload_file_t *upload_file = NULL;
    aos_string_t upload_id;
    aos_table_t *resp_headers = NULL;
    aos_status_t *resp_status = NULL;
    int64_t part_num = 1;
    char* etag = NULL;

    aos_str_set(&bucket, bucket_name);
    aos_str_set(&object, object_name);
    aos_str_set(&upload_id, upload_id_str);
    part_num = aos_atoi64(part_num_str);

    /* 上传分片。*/
    upload_file = oss_create_upload_file(pool);
    aos_str_set(&upload_file->filename, local_filename);
    upload_file->file_pos = 0;
    upload_file->file_last = get_file_size(local_filename);
    resp_status = oss_do_upload_part_from_file(oss_client_options,
                                               &bucket,
                                               &object,
                                               &upload_id,
                                               part_num,
                                               upload_file,
                                               percentage,
                                               NULL,
                                               NULL,
                                               &resp_headers,
                                               NULL);

    etag = apr_pstrdup(pool, (char*)apr_table_get(resp_headers, "ETag"));
    printf("\npart-number: %ld, etag: %s\n", part_num, etag);

    if (aos_status_is_ok(resp_status)) {
        printf("Multipart upload part from file succeeded\n");
    } else {
        printf("Multipart upload part from file failed\n");
    }

    /* 释放内存池，相当于释放了请求过程中各资源分配的内存。*/
    aos_pool_destroy(pool);
    /* 释放之前分配的全局资源。*/
    aos_http_io_deinitialize();

    myoss_free();

    return 0;
}
