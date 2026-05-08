#include "http.h"

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *dest, int dest_size, const char *src)
{
    utils_safe_copy(dest, dest_size, src);
}

void http_request_init(HttpRequest *request)
{
    if (request == NULL) {
        return;
    }

    memset(request, 0, sizeof(HttpRequest));
}

static const char *find_body_start(const char *raw)
{
    const char *p;

    if (raw == NULL) {
        return NULL;
    }

    p = strstr(raw, "\r\n\r\n");
    if (p != NULL) {
        return p + 4;
    }

    p = strstr(raw, "\n\n");
    if (p != NULL) {
        return p + 2;
    }

    return NULL;
}

static int read_line(const char **cursor, char *line, int line_size)
{
    int i = 0;
    const char *p;

    if (cursor == NULL || *cursor == NULL || line == NULL || line_size <= 0) {
        return 0;
    }

    p = *cursor;
    if (*p == '\0') {
        line[0] = '\0';
        return 0;
    }

    while (*p != '\0' && *p != '\r' && *p != '\n' && i < line_size - 1) {
        line[i++] = *p++;
    }
    line[i] = '\0';

    if (*p == '\r') {
        ++p;
    }
    if (*p == '\n') {
        ++p;
    }

    *cursor = p;
    return 1;
}

static void split_path_and_query(const char *url, HttpRequest *request)
{
    const char *question;
    int path_len;

    if (url == NULL || request == NULL) {
        return;
    }

    question = strchr(url, '?');
    if (question == NULL) {
        copy_text(request->path, HTTP_PATH_LEN, url);
        request->query[0] = '\0';
        return;
    }

    path_len = (int)(question - url);
    if (path_len >= HTTP_PATH_LEN) {
        path_len = HTTP_PATH_LEN - 1;
    }

    strncpy(request->path, url, (size_t)path_len);
    request->path[path_len] = '\0';
    copy_text(request->query, HTTP_QUERY_LEN, question + 1);
}

static int parse_request_line(const char *line, HttpRequest *request)
{
    char url[HTTP_PATH_LEN + HTTP_QUERY_LEN];

    if (line == NULL || request == NULL) {
        return 0;
    }

    if (sscanf(line, "%7s %1279s %15s", request->method, url, request->version) != 3) {
        return 0;
    }

    split_path_and_query(url, request);
    if (request->path[0] == '\0') {
        copy_text(request->path, HTTP_PATH_LEN, "/");
    }

    return 1;
}

static int add_header(HttpRequest *request, const char *line)
{
    const char *colon;
    int name_len;

    if (request == NULL || line == NULL || request->header_count >= HTTP_MAX_HEADERS) {
        return 0;
    }

    colon = strchr(line, ':');
    if (colon == NULL) {
        return 0;
    }

    name_len = (int)(colon - line);
    if (name_len >= HTTP_HEADER_NAME_LEN) {
        name_len = HTTP_HEADER_NAME_LEN - 1;
    }

    strncpy(request->headers[request->header_count].name, line, (size_t)name_len);
    request->headers[request->header_count].name[name_len] = '\0';
    copy_text(request->headers[request->header_count].value,
              HTTP_HEADER_VALUE_LEN,
              colon + 1);
    utils_trim(request->headers[request->header_count].name);
    utils_trim(request->headers[request->header_count].value);
    ++request->header_count;
    return 1;
}

int http_parse_request(const char *raw, int raw_length, HttpRequest *request)
{
    const char *cursor;
    const char *body_start;
    char line[2048];

    if (raw == NULL || raw_length <= 0 || request == NULL) {
        return 0;
    }

    http_request_init(request);
    cursor = raw;

    if (!read_line(&cursor, line, sizeof(line)) || !parse_request_line(line, request)) {
        return 0;
    }

    while (read_line(&cursor, line, sizeof(line))) {
        if (line[0] == '\0') {
            break;
        }
        add_header(request, line);
    }

    body_start = find_body_start(raw);
    if (body_start != NULL) {
        int header_length = (int)(body_start - raw);
        int body_length = raw_length - header_length;
        if (body_length < 0) {
            body_length = 0;
        }
        if (body_length >= HTTP_BODY_LEN) {
            body_length = HTTP_BODY_LEN - 1;
        }
        memcpy(request->body, body_start, (size_t)body_length);
        request->body[body_length] = '\0';
        request->body_length = body_length;
    }

    return 1;
}

const char *http_get_header(const HttpRequest *request, const char *name)
{
    int i;

    if (request == NULL || name == NULL) {
        return NULL;
    }

    for (i = 0; i < request->header_count; ++i) {
        if (utils_equals_ignore_case(request->headers[i].name, name)) {
            return request->headers[i].value;
        }
    }

    return NULL;
}

int http_get_query_param(const HttpRequest *request, const char *key, char *value, int value_size)
{
    if (request == NULL) {
        return 0;
    }
    return utils_get_query_param(request->query, key, value, value_size);
}

int http_get_form_value(const HttpRequest *request, const char *key, char *value, int value_size)
{
    if (request == NULL) {
        return 0;
    }
    return utils_get_form_value(request->body, key, value, value_size);
}

int http_is_method(const HttpRequest *request, const char *method)
{
    if (request == NULL || method == NULL) {
        return 0;
    }
    return utils_equals_ignore_case(request->method, method);
}

void http_response_init(HttpResponse *response)
{
    if (response == NULL) {
        return;
    }

    memset(response, 0, sizeof(HttpResponse));
    response->status_code = 200;
    copy_text(response->status_text, sizeof(response->status_text), "OK");
    copy_text(response->content_type, sizeof(response->content_type), "text/plain; charset=utf-8");
}

const char *http_status_text(int status_code)
{
    switch (status_code) {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 302:
        return "Found";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 500:
        return "Internal Server Error";
    default:
        return "OK";
    }
}

void http_set_status(HttpResponse *response, int status_code)
{
    if (response == NULL) {
        return;
    }

    response->status_code = status_code;
    copy_text(response->status_text, sizeof(response->status_text), http_status_text(status_code));
}

int http_set_body(HttpResponse *response, const char *content_type, const char *body)
{
    if (body == NULL) {
        body = "";
    }
    return http_set_body_bytes(response, content_type, body, (int)strlen(body));
}

int http_set_body_bytes(HttpResponse *response, const char *content_type, const char *body, int body_length)
{
    if (response == NULL || body == NULL || body_length < 0) {
        return 0;
    }
    if (body_length >= HTTP_RESPONSE_LEN) {
        body_length = HTTP_RESPONSE_LEN - 1;
    }

    if (!utils_is_blank(content_type)) {
        copy_text(response->content_type, sizeof(response->content_type), content_type);
    }

    memcpy(response->body, body, (size_t)body_length);
    response->body[body_length] = '\0';
    response->body_length = body_length;
    return 1;
}

int http_build_response(HttpResponse *response)
{
    int header_length;

    if (response == NULL) {
        return 0;
    }

    header_length = snprintf(response->raw,
                             sizeof(response->raw),
                             "HTTP/1.1 %d %s\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %d\r\n"
                             "Connection: close\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                             "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                             "\r\n",
                             response->status_code,
                             response->status_text,
                             response->content_type,
                             response->body_length);

    if (header_length < 0 || header_length + response->body_length >= (int)sizeof(response->raw)) {
        return 0;
    }

    memcpy(response->raw + header_length, response->body, (size_t)response->body_length);
    response->raw_length = header_length + response->body_length;
    response->raw[response->raw_length] = '\0';
    return 1;
}

int http_response_text(HttpResponse *response, int status_code, const char *text)
{
    http_response_init(response);
    http_set_status(response, status_code);
    return http_set_body(response, "text/plain; charset=utf-8", text) && http_build_response(response);
}

int http_response_html(HttpResponse *response, int status_code, const char *html)
{
    http_response_init(response);
    http_set_status(response, status_code);
    return http_set_body(response, "text/html; charset=utf-8", html) && http_build_response(response);
}

int http_response_json(HttpResponse *response, int status_code, const char *json)
{
    http_response_init(response);
    http_set_status(response, status_code);
    return http_set_body(response, "application/json; charset=utf-8", json) && http_build_response(response);
}

int http_response_error(HttpResponse *response, int status_code, const char *message)
{
    char escaped[1024];
    char json[1400];

    if (utils_is_blank(message)) {
        message = http_status_text(status_code);
    }

    utils_json_escape(message, escaped, sizeof(escaped));
    snprintf(json, sizeof(json), "{\"success\":false,\"message\":\"%s\"}", escaped);
    return http_response_json(response, status_code, json);
}

int http_response_redirect(HttpResponse *response, const char *location)
{
    int header_length;

    if (response == NULL || utils_is_blank(location)) {
        return 0;
    }

    http_response_init(response);
    http_set_status(response, 302);
    response->body_length = 0;
    header_length = snprintf(response->raw,
                             sizeof(response->raw),
                             "HTTP/1.1 302 Found\r\n"
                             "Location: %s\r\n"
                             "Content-Length: 0\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             location);
    if (header_length < 0 || header_length >= (int)sizeof(response->raw)) {
        return 0;
    }
    response->raw_length = header_length;
    return 1;
}

int http_response_file(HttpResponse *response, const char *filename)
{
    FILE *fp;
    long file_size;
    int read_size;
    const char *mime;

    if (response == NULL || utils_is_blank(filename)) {
        return 0;
    }

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return http_response_error(response, 404, "文件不存在");
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0 || file_size >= HTTP_RESPONSE_LEN) {
        fclose(fp);
        return http_response_error(response, 500, "文件过大，无法返回");
    }

    http_response_init(response);
    http_set_status(response, 200);
    mime = utils_get_mime_type(filename);
    read_size = (int)fread(response->body, 1, (size_t)file_size, fp);
    fclose(fp);

    response->body_length = read_size;
    response->body[read_size] = '\0';
    copy_text(response->content_type, sizeof(response->content_type), mime);
    return http_build_response(response);
}
