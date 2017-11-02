
/*
 * Copyright (C) Kit C. Dallege
 *
 * This module provides run-time image modification via transformations
 * which are encoded in the images file name.
 *
 * Optional security features such as a white_list &
 * key ('used as a salt for an md5 hash parameter') are also provided.
 *
 * Examples of uri's with encoded transformations:
 * docroot/img/test.jpg
 * docroot/img/test_t400.jpg
 * docroot/img/test_t200x400.jpg
 * docroot/img/test_r400x400.jpg
 * docroot/img/test_r220.jpg
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* strtok */
#include <string.h>

/* Graphics Magick API Wrapper */
#include "imaging.h"

/* Configuration options type */
typedef struct {
    ngx_str_t   salt;
    ngx_uint_t  quality;
    ngx_str_t   white_list;
    ngx_flag_t  write_to_disk;

} ngx_http_imaging_loc_conf_t;

/* Functions prototypes. */
static char *ngx_http_imaging(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_imaging_create_conf(ngx_conf_t *cf);
static char *ngx_http_imaging_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);
ngx_int_t ngx_http_imaging_at_init(ngx_cycle_t *cycle);
void ngx_http_imaging_at_exit(ngx_cycle_t *cycle);

/* Available configuration parameters */
static ngx_command_t ngx_http_imaging_commands[] = {
    { ngx_string("imaging"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
          |NGX_HTTP_LIF_CONF|NGX_CONF_FLAG,
      ngx_http_imaging,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("imaging_salt"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_imaging_loc_conf_t, salt),
      NULL },

    { ngx_string("imaging_quality"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_imaging_loc_conf_t, quality),
      NULL },

    { ngx_string("imaging_white_list"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_imaging_loc_conf_t, white_list),
      NULL },

    { ngx_string("imaging_write_to_disk"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_imaging_loc_conf_t, write_to_disk),
      NULL },

    ngx_null_command
};

/*
 * Module Context
 */
static ngx_http_module_t ngx_http_imaging_module_ctx = {
    NULL,                          /* pre-configuration */
    NULL,                          /* post-configuration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    ngx_http_imaging_create_conf,  /* create location configuration */
    ngx_http_imaging_merge_conf    /* merge location configuration */
};

/*
 * Module Configuration & Registration.
 */
ngx_module_t ngx_http_imaging_module = {
    NGX_MODULE_V1,
    &ngx_http_imaging_module_ctx,    /* module context */
    ngx_http_imaging_commands,       /* module directives */
    NGX_HTTP_MODULE,                 /* module type */
    NULL,                            /* init master */
    NULL,                            /* init module */
    ngx_http_imaging_at_init,        /* init process */
    NULL,                            /* init thread */
    NULL,                            /* exit thread */
    ngx_http_imaging_at_exit,        /* exit process */
    NULL,                            /* exit master */
    NGX_MODULE_V1_PADDING
};

/*
 * ngx_http_request_t handler which creates images from transformations
 * encoded in the request path.
 *
 * Eg:
 *  If img.jpg is the root image, than a request for img_t200x200.jpg would
 *  create a 200 by 200 thumbnail of img.jpg.
 */
static ngx_int_t
ngx_http_imaging_handler(ngx_http_request_t *request)
{
    u_char                        *last;
    size_t                         root;
    ngx_str_t                      path;
    ngx_int_t                      rc;   
    ngx_buf_t                     *buffer;
    ngx_chain_t                    out;
    ngx_http_imaging_loc_conf_t   *conf;
    unsigned char                 *buf_data;
    unsigned char                 *data;
    unsigned char                 *content_type;
    char                          *mime_type;
    char                          *hash;
    size_t                         data_length;
    size_t                         content_type_len;
#if (NGX_DEBUG)    
    ngx_log_t                     *log;
#endif

    /* only respond to 'GET' and 'HEAD' requests. */
    if (!(request->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    /* only process image files. */
    if (request->uri.data[request->uri.len - 1] == '/') {
        return NGX_DECLINED;
    }

    /* last component of the uri */
    last = ngx_http_map_uri_to_path(request, &path, &root, 0);

    if (last == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    path.len = last - path.data;

#if (NGX_DEBUG)
    /* local log ref */
    log = request->connection->log;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                    "image filename: \"%s\"", path.data);
#endif

    /* load configs */
    conf = ngx_http_get_module_loc_conf(request, ngx_http_imaging_module);


    /* discard request body, since we'll be creating it don't need it here */
    rc = ngx_http_discard_request_body(request);

    if (rc != NGX_OK) {
        return rc;
    }

    /* init locals */
    data = buf_data = content_type = NULL;
    hash = mime_type = NULL;
    content_type_len = data_length = 0;
    /* pull hash out of request args */
    hash = malloc(request->args.len + 1);
    strncpy(hash, (char *)request->args.data, request->args.len);
    hash[request->args.len] = '\0';
    imgaging_get_image_data(
        (const char *)path.data,
        &data, &data_length,
        &mime_type, &content_type_len,
        (const char *)conf->salt.data,
        (const char *)hash,
        conf->quality,
        (const char *)conf->white_list.data,
        conf->write_to_disk
    );

    // if we failed to create the image log about it.
    if (data == NULL) {
#if (NGX_DEBUG)    
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "failed to create: %s", path.data);
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "salt: '%s'", conf->salt.data);
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "hash: '%s'", hash);
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "quality: '%d'", conf->quality);
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "write_to_disk: '%s'", conf->write_to_disk?"true":"false");
#endif
        free(hash);
        return NGX_HTTP_NOT_FOUND;
    }

#if (NGX_DEBUG)
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "created: \"%s\"", path.data);
#endif

    // put data under the request pools memory management.
    buf_data = ngx_pcalloc(request->pool, data_length);
    ngx_memcpy(buf_data, data, data_length);
    free(data);

    // create buffer
    buffer = ngx_pcalloc(request->pool, sizeof(ngx_buf_t));
    if (buffer == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* add data to buffer  */
    buffer->pos = buf_data;
    buffer->last = buf_data + data_length;
    buffer->memory = 1;    /* this buffer is in memory (read-only) */
    buffer->last_buf = 1;  /* this is the last buffer in the buffer chain */

    /* attach this buffer to the buffer chain */
    out.buf = buffer;
    out.next = NULL;

    // put mime_type/content_type  under the request pools memory management.
    content_type = ngx_pcalloc(request->pool, content_type_len);
    ngx_memcpy(content_type, mime_type, content_type_len);
    free(mime_type);

    /* set the 'Content-type' header */
    request->headers_out.content_type_len = content_type_len;
    request->headers_out.content_type.data = (u_char *) content_type;

    /* set request status to 200 & the content-length */
    request->headers_out.status = NGX_HTTP_OK;
    request->headers_out.content_length_n = data_length;

    /* send the headers of the response */
    rc = ngx_http_send_header(request);

    /*
     * if the request type is 'HEAD' (or there was an error) return the
     * return code from 'send_header' as the headers have already been sent.
     */
    if (request->method == NGX_HTTP_HEAD || request->header_only ||
        rc == NGX_ERROR || rc > NGX_OK) {
        return rc;
    }

    /* send the buffer chain of your response */
    return ngx_http_output_filter(request, &out);
}

/*
 * Register `ngx_http_imaging_handler` with the local configuration.
 */
static char *
ngx_http_imaging(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_imaging_handler; /* hook our handler up */

    return NGX_CONF_OK;
}

/*
 * Create ngx_http_imaging_loc_conf_t instance.
 */
static void *
ngx_http_imaging_create_conf(ngx_conf_t *cf)
{
    ngx_http_imaging_loc_conf_t *conf;
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_imaging_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /* set by ngx_pcalloc
     * conf->salt = {0, NULL};
     * conf->white_list = {0, NULL};
     */
    conf->quality = NGX_CONF_UNSET_UINT;
    conf->write_to_disk = NGX_CONF_UNSET;
    return conf;
}

/*
 * Merge ngx_http_imaging_loc_conf_t instances together.
 */
static char *
ngx_http_imaging_merge_conf(ngx_conf_t *cf, void *parent,void *child)
{
    ngx_http_imaging_loc_conf_t *prev = parent;
    ngx_http_imaging_loc_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->salt, prev->salt, "");
    ngx_conf_merge_uint_value(conf->quality, prev->quality, 70);
    ngx_conf_merge_str_value(conf->white_list, prev->white_list, "");
    ngx_conf_merge_value(conf->write_to_disk, prev->write_to_disk, 1);

    return NGX_CONF_OK;
}


ngx_int_t
ngx_http_imaging_at_init(ngx_cycle_t *cycle)
{
    imaging_initialize();
    return NGX_OK;
}

void
ngx_http_imaging_at_exit(ngx_cycle_t *cycle)
{
    imaging_destory();
}

