#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/* 通用布尔宏，避免依赖 C99 stdbool 时不同编译器配置出问题。 */
#define APP_TRUE 1
#define APP_FALSE 0

/* 基础字符串工具 */
int utils_is_blank(const char *text);
void utils_safe_copy(char *dest, int dest_size, const char *src);
void utils_trim(char *text);
int utils_equals_ignore_case(const char *a, const char *b);
int utils_contains_ignore_case(const char *source, const char *keyword);

/* 时间与 ID 工具 */
void utils_current_time(char *buffer, int size);
int utils_next_int_id(int current_max);

/* HTTP/Web 后端常用编码工具 */
int utils_hex_value(char ch);
void utils_url_decode(const char *src, char *dest, int dest_size);
void utils_url_encode(const char *src, char *dest, int dest_size);
int utils_get_query_param(const char *query, const char *key, char *value, int value_size);
int utils_get_form_value(const char *body, const char *key, char *value, int value_size);
void utils_json_escape(const char *src, char *dest, int dest_size);

/* 文件路径和 MIME 类型工具 */
void utils_join_path(char *dest, int dest_size, const char *dir, const char *name);
const char *utils_get_mime_type(const char *path);
long utils_file_size(const char *filename);
int utils_read_text_file(const char *filename, char *buffer, int buffer_size);

#endif
