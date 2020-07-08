#include "myoss.h"

char *bucket_name;
char *object_name;
char *upload_id_str;

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "multipart-complete <bucket> <object> <upload-id>\n");
        exit(1);
    }
    bucket_name = argv[1];
    object_name = argv[2];
    upload_id_str = argv[3];

    /* 在程序入口调用aos_http_io_initialize方法来初始化网络、内存等全局资源。*/
    if (aos_http_io_initialize(NULL, 0) != AOSE_OK) {
        exit(1);
    }
    /* 用于内存管理的内存池（pool），等价于apr_pool_t。其实现代码在apr库中。*/
    aos_pool_t *pool;
    aos_table_t *complete_headers = NULL;

    /* 重新创建一个内存池，第二个参数是NULL，表示没有继承其它内存池。*/
    aos_pool_create(&pool, NULL);
    complete_headers = aos_table_make(pool, 1);

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
    aos_table_t *resp_headers = NULL;
    aos_status_t *resp_status = NULL;
    oss_list_upload_part_params_t *params = NULL;
    oss_list_part_content_t *part_content = NULL;
    oss_complete_part_content_t *complete_part_content = NULL;
    aos_list_t complete_part_list;

    aos_str_set(&bucket, bucket_name);
    aos_str_set(&object, object_name);
    aos_str_set(&upload_id, upload_id_str);
    params = oss_create_list_upload_part_params(pool);
    params->max_ret = 1000;

    resp_status = oss_list_upload_part(oss_client_options,
                                       &bucket,
                                       &object,
                                       &upload_id,
                                       params,
                                       &resp_headers);

    if (aos_status_is_ok(resp_status)) {
        printf("List multipart succeeded\n");
    } else {
        printf("List multipart failed\n");
        exit(1);
    }

    aos_list_init(&complete_part_list);
    aos_list_for_each_entry(oss_list_part_content_t, part_content, &params->part_list, node) {
        complete_part_content = oss_create_complete_part_content(pool);
        printf("part-number: %s, etag: %s\n", part_content->part_number.data, part_content->etag.data);
        aos_str_set(&complete_part_content->part_number,
                    part_content->part_number.data);
        aos_str_set(&complete_part_content->etag, part_content->etag.data);
        aos_list_add_tail(&complete_part_content->node, &complete_part_list);
    }

    resp_status = oss_complete_multipart_upload(oss_client_options,
                                                &bucket,
                                                &object,
                                                &upload_id,
                                                &complete_part_list,
                                                complete_headers,
                                                &resp_headers);

    if (aos_status_is_ok(resp_status)) {
        printf("Complete multipart upload succeeded, upload_id: %.*s\n",
               upload_id.len, upload_id.data);
    } else {
        printf("Complete multipart upload failed\n");
        exit(1);
    }

    /* 释放内存池，相当于释放了请求过程中各资源分配的内存。*/
    aos_pool_destroy(pool);
    /* 释放之前分配的全局资源。*/
    aos_http_io_deinitialize();

    myoss_free();

    return 0;
}
