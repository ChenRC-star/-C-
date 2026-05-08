#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_METHOD_LEN       8
#define HTTP_PATH_LEN         260
#define HTTP_VERSION_LEN      16
#define HTTP_QUERY_LEN        1024
#define HTTP_BODY_LEN         8192
#define HTTP_HEADER_NAME_LEN  64
#define HTTP_HEADER_VALUE_LEN 512
#define HTTP_MAX_HEADERS      32
#define HTTP_RESPONSE_LEN     65536

/* 单个 HTTP 请求头。 */
typedef struct HttpHeader {
    char name[HTTP_HEADER_NAME_LEN];
    char value[HTTP_HEADER_VALUE_LEN];
} HttpHeader;

/* 解析后的 HTTP 请求。 */
typedef struct HttpRequest {
    char method[HTTP_METHOD_LEN];              /* GET / POST 等 */
    char path[HTTP_PATH_LEN];                  /* 不含 query 的路径，如 /api/books */
    char query[HTTP_QUERY_LEN];                /* ? 后面的查询字符串 */
    char version[HTTP_VERSION_LEN];            /* HTTP/1.1 */
    HttpHeader headers[HTTP_MAX_HEADERS];      /* 请求头数组 */
    int header_count;                          /* 请求头数量 */
    char body[HTTP_BODY_LEN];                  /* POST 请求体 */
    int body_length;                           /* 请求体长度 */
} HttpRequest;

/* 待发送的 HTTP 响应。 */
typedef struct HttpResponse {
    int status_code;                           /* 200 / 404 / 500 等 */
    char status_text[64];                      /* OK / Not Found 等 */
    char content_type[128];                    /* text/html / application/json */
    char body[HTTP_RESPONSE_LEN];              /* 响应正文 */
    int body_length;                           /* 正文长度 */
    char raw[HTTP_RESPONSE_LEN + 1024];        /* 完整 HTTP 响应报文 */
    int raw_length;                            /* 完整响应长度 */
} HttpResponse;

/* 请求解析 */
void http_request_init(HttpRequest *request);
int http_parse_request(const char *raw, int raw_length, HttpRequest *request);
const char *http_get_header(const HttpRequest *request, const char *name);
int http_get_query_param(const HttpRequest *request, const char *key, char *value, int value_size);
int http_get_form_value(const HttpRequest *request, const char *key, char *value, int value_size);
int http_is_method(const HttpRequest *request, const char *method);

/* 响应构造 */
void http_response_init(HttpResponse *response);
const char *http_status_text(int status_code);
void http_set_status(HttpResponse *response, int status_code);
int http_set_body(HttpResponse *response, const char *content_type, const char *body);
int http_set_body_bytes(HttpResponse *response, const char *content_type, const char *body, int body_length);
int http_build_response(HttpResponse *response);

/* 常用响应快捷函数 */
int http_response_text(HttpResponse *response, int status_code, const char *text);
int http_response_html(HttpResponse *response, int status_code, const char *html);
int http_response_json(HttpResponse *response, int status_code, const char *json);
int http_response_error(HttpResponse *response, int status_code, const char *message);
int http_response_redirect(HttpResponse *response, const char *location);
int http_response_file(HttpResponse *response, const char *filename);

#endif
