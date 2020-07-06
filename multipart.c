#include "myoss.h"

char *bucket_name;
char *object_name;
char *local_filename;

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "multipart <bucket> <local-file> <object>\n");
        exit(1);
    }
    bucket_name = argv[1];
    local_filename = argv[2];
    object_name = argv[3];

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
    aos_table_t *headers = NULL;
    aos_table_t *complete_headers = NULL;
    aos_table_t *resp_headers = NULL;
    aos_status_t *resp_status = NULL;

    aos_str_set(&bucket, bucket_name);
    aos_str_set(&object, object_name);
    aos_str_null(&upload_id);
    headers = aos_table_make(pool, 1);
    complete_headers = aos_table_make(pool, 1);
    int part_num = 1;

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

    /* 上传分片。*/
    int64_t file_length = 0;
    int64_t pos = 0;
    aos_list_t complete_part_list;
    oss_complete_part_content_t* complete_content = NULL;
    char* part_num_str = NULL;
    char* etag = NULL;

    aos_list_init(&complete_part_list);

    file_length = get_file_size(local_filename);
    while(pos < file_length) {
        upload_file = oss_create_upload_file(pool);
        aos_str_set(&upload_file->filename, local_filename);
        upload_file->file_pos = pos;
        pos += 1024 * 1024;
        upload_file->file_last = pos < file_length ? pos : file_length;
        resp_status = oss_upload_part_from_file(oss_client_options, &bucket, &object, &upload_id, part_num++, upload_file, &resp_headers);

        /* 保存分片号和Etag。*/
        complete_content = oss_create_complete_part_content(pool);
        part_num_str = apr_psprintf(pool, "%d", part_num-1);
        aos_str_set(&complete_content->part_number, part_num_str);
        etag = apr_pstrdup(pool, (char*)apr_table_get(resp_headers, "ETag"));
        aos_str_set(&complete_content->etag, etag);
        aos_list_add_tail(&complete_content->node, &complete_part_list);

        if (aos_status_is_ok(resp_status)) {
            printf("Multipart upload part from file succeeded\n");
        } else {
            printf("Multipart upload part from file failed\n");
        }
    }

    /* 完成分片上传。*/
    resp_status = oss_complete_multipart_upload(oss_client_options, &bucket, &object, &upload_id, &complete_part_list, complete_headers, &resp_headers);
    /* 判断分片上传是否完成。*/
    if (aos_status_is_ok(resp_status)) {
        printf("Complete multipart upload from file succeeded, upload_id: %.*s\n", 
               upload_id.len, upload_id.data);
    } else {
        printf("Complete multipart upload from file failed\n");
    }

    /* 释放内存池，相当于释放了请求过程中各资源分配的内存。*/
    aos_pool_destroy(pool);
    /* 释放之前分配的全局资源。*/
    aos_http_io_deinitialize();

    myoss_free();

    return 0;
}
