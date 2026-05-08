#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================= 基础字符串处理 ================= */

/** @brief 检查字符串是否为 NULL 或内容为空 */
int utils_is_blank(const char *text)
{
    return text == NULL || text[0] == '\0';
}

/** * @brief 安全复制字符串。
 * 显式强制设置 '\0'，有效防止 strncpy 不自动补断句符导致的缓冲区溢出。
 */
void utils_safe_copy(char *dest, int dest_size, const char *src)
{
    if (dest == NULL || dest_size <= 0) {
        return;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, (size_t)dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/** * @brief 去除字符串首尾的空白字符（空格、制表符、换行等）。
 * 使用 memmove 处理重叠区域，实现原地修改。
 */
void utils_trim(char *text)
{
    char *start;
    char *end;
    size_t len;

    if (text == NULL) {
        return;
    }

    start = text;
    // 寻找第一个非空白字符
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    len = strlen(text);
    if (len == 0) {
        return;
    }

    // 从尾部向前寻找第一个非空白字符
    end = text + len - 1;
    while (end >= text && isspace((unsigned char)*end)) {
        *end = '\0';
        if (end == text) {
            break;
        }
        --end;
    }
}

/** @brief 不区分大小写的字符串比较 */
int utils_equals_ignore_case(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return a == b;
    }

    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

/** @brief 在 source 中查找 keyword，忽略大小写（常用于搜索功能） */
int utils_contains_ignore_case(const char *source, const char *keyword)
{
    size_t i, j, source_len, keyword_len;

    if (utils_is_blank(keyword)) return 1;
    if (utils_is_blank(source)) return 0;

    source_len = strlen(source);
    keyword_len = strlen(keyword);
    if (keyword_len > source_len) return 0;

    for (i = 0; i <= source_len - keyword_len; ++i) {
        for (j = 0; j < keyword_len; ++j) {
            if (tolower((unsigned char)source[i + j]) != tolower((unsigned char)keyword[j])) {
                break;
            }
        }
        if (j == keyword_len) return 1;
    }
    return 0;
}

/* ================= 时间与 ID 工具 ================= */

/** @brief 获取当前格式化时间：YYYY-MM-DD HH:MM:SS */
void utils_current_time(char *buffer, int size)
{
    time_t now;
    struct tm *local_time;

    if (buffer == NULL || size <= 0) return;

    now = time(NULL);
    local_time = localtime(&now);
    if (local_time == NULL) {
        buffer[0] = '\0';
        return;
    }

    strftime(buffer, (size_t)size, "%Y-%m-%d %H:%M:%S", local_time);
}

/** @brief 简单的自增 ID 生成逻辑 */
int utils_next_int_id(int current_max)
{
    return current_max < 0 ? 1 : current_max + 1;
}

/* ================= Web/HTTP 相关编解码 ================= */

/** @brief 十六进制字符转整数 (e.g., 'A' -> 10) */
int utils_hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

/** * @brief URL 解码。将 %E4%BD%A0 转换回原始字符，将 '+' 转为空格。
 */
void utils_url_decode(const char *src, char *dest, int dest_size)
{
    int i = 0;
    if (dest == NULL || dest_size <= 0) return;
    dest[0] = '\0';
    if (src == NULL) return;

    while (*src != '\0' && i < dest_size - 1) {
        if (*src == '%' && utils_hex_value(src[1]) >= 0 && utils_hex_value(src[2]) >= 0) {
            // 处理 %HH 格式
            dest[i++] = (char)(utils_hex_value(src[1]) * 16 + utils_hex_value(src[2]));
            src += 3;
        } else if (*src == '+') {
            // URL 标准中 + 代表空格
            dest[i++] = ' ';
            ++src;
        } else {
            dest[i++] = *src++;
        }
    }
    dest[i] = '\0';
}

/** @brief URL 编码。保护特殊字符，防止请求参数被破坏 */
void utils_url_encode(const char *src, char *dest, int dest_size)
{
    static const char hex[] = "0123456789ABCDEF";
    int i = 0;
    if (dest == NULL || dest_size <= 0) return;
    dest[0] = '\0';
    if (src == NULL) return;

    while (*src != '\0' && i < dest_size - 1) {
        unsigned char ch = (unsigned char)*src++;
        // 字母、数字及部分安全符号不编码
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            dest[i++] = (char)ch;
        } else if (ch == ' ') {
            dest[i++] = '+';
        } else if (i + 3 < dest_size) {
            dest[i++] = '%';
            dest[i++] = hex[ch >> 4];
            dest[i++] = hex[ch & 15];
        } else {
            break;
        }
    }
    dest[i] = '\0';
}

/** * @brief 内部函数：解析 key=value 键值对字符串。
 * 兼容 HTTP GET QueryString 和 POST form-urlencoded 格式。
 */
static int get_pair_value(const char *text, const char *key, char *value, int value_size)
{
    const char *p = text;
    int key_len;

    if (utils_is_blank(text) || utils_is_blank(key) || value == NULL || value_size <= 0) return 0;

    key_len = (int)strlen(key);
    value[0] = '\0';

    while (*p != '\0') {
        const char *equal = strchr(p, '=');
        const char *amp = strchr(p, '&');
        int name_len;
        char encoded[1024];

        if (equal == NULL) return 0;
        // 如果 & 在 = 前面，说明是一个空的或者错误的参数，跳过
        if (amp != NULL && amp < equal) {
            p = amp + 1;
            continue;
        }

        name_len = (int)(equal - p);
        if (name_len == key_len && strncmp(p, key, (size_t)key_len) == 0) {
            int raw_len;
            const char *value_start = equal + 1;
            amp = strchr(value_start, '&');
            raw_len = amp == NULL ? (int)strlen(value_start) : (int)(amp - value_start);
            
            if (raw_len >= (int)sizeof(encoded)) raw_len = (int)sizeof(encoded) - 1;
            
            strncpy(encoded, value_start, (size_t)raw_len);
            encoded[raw_len] = '\0';
            // 解析出的值必须经过 URL 解码
            utils_url_decode(encoded, value, value_size);
            return 1;
        }

        amp = strchr(equal + 1, '&');
        if (amp == NULL) break;
        p = amp + 1;
    }
    return 0;
}

/** @brief 从 URL 路径中解析参数 (e.g., ?id=123) */
int utils_get_query_param(const char *query, const char *key, char *value, int value_size)
{
    const char *real_query = query;
    if (real_query != NULL && real_query[0] == '?') ++real_query;
    return get_pair_value(real_query, key, value, value_size);
}

/** @brief 从 HTTP 请求体中解析表单字段 */
int utils_get_form_value(const char *body, const char *key, char *value, int value_size)
{
    return get_pair_value(body, key, value, value_size);
}

/** * @brief JSON 转义处理。
 * 将控制字符（回车、换行、引号等）转为 \n, \", \\ 格式，确保生成的 JSON 字符串合法。
 */
void utils_json_escape(const char *src, char *dest, int dest_size)
{
    int i = 0;
    if (dest == NULL || dest_size <= 0) return;
    dest[0] = '\0';
    if (src == NULL) return;

    while (*src != '\0' && i < dest_size - 1) {
        unsigned char ch = (unsigned char)*src++;
        if ((ch == '"' || ch == '\\') && i + 2 < dest_size) {
            dest[i++] = '\\'; dest[i++] = (char)ch;
        } else if (ch == '\n' && i + 2 < dest_size) {
            dest[i++] = '\\'; dest[i++] = 'n';
        } else if (ch == '\r' && i + 2 < dest_size) {
            dest[i++] = '\\'; dest[i++] = 'r';
        } else if (ch == '\t' && i + 2 < dest_size) {
            dest[i++] = '\\'; dest[i++] = 't';
        } else {
            dest[i++] = (char)ch;
        }
    }
    dest[i] = '\0';
}

/* ================= 文件与系统工具 ================= */

/** @brief 拼接目录和文件名，处理路径分隔符 */
void utils_join_path(char *dest, int dest_size, const char *dir, const char *name)
{
    int dir_len;
    const char *real_dir = dir == NULL ? "" : dir;
    const char *real_name = name == NULL ? "" : name;

    if (dest == NULL || dest_size <= 0) return;

    dir_len = (int)strlen(real_dir);
    if (dir_len > 0 && (real_dir[dir_len - 1] == '\\' || real_dir[dir_len - 1] == '/')) {
        snprintf(dest, (size_t)dest_size, "%s%s", real_dir, real_name);
    } else {
        snprintf(dest, (size_t)dest_size, "%s/%s", real_dir, real_name);
    }
}

/** @brief 根据文件后缀返回 HTTP Content-Type */
const char *utils_get_mime_type(const char *path)
{
    const char *ext;
    if (path == NULL) return "application/octet-stream";

    ext = strrchr(path, '.');
    if (ext == NULL) return "application/octet-stream";

    if (utils_equals_ignore_case(ext, ".html") || utils_equals_ignore_case(ext, ".htm")) return "text/html; charset=utf-8";
    if (utils_equals_ignore_case(ext, ".css")) return "text/css; charset=utf-8";
    if (utils_equals_ignore_case(ext, ".js")) return "application/javascript; charset=utf-8";
    if (utils_equals_ignore_case(ext, ".json")) return "application/json; charset=utf-8";
    if (utils_equals_ignore_case(ext, ".png")) return "image/png";
    if (utils_equals_ignore_case(ext, ".jpg") || utils_equals_ignore_case(ext, ".jpeg")) return "image/jpeg";
    if (utils_equals_ignore_case(ext, ".svg")) return "image/svg+xml";

    return "application/octet-stream";
}

/** @brief 获取物理文件字节大小 */
long utils_file_size(const char *filename)
{
    FILE *fp;
    long size;
    if (utils_is_blank(filename)) return -1;

    fp = fopen(filename, "rb");
    if (fp == NULL) return -1;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fclose(fp);
    return size;
}

/** @brief 将整个文本文件内容读取到内存缓冲区 */
int utils_read_text_file(const char *filename, char *buffer, int buffer_size)
{
    FILE *fp;
    size_t read_size;

    if (utils_is_blank(filename) || buffer == NULL || buffer_size <= 0) return 0;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        buffer[0] = '\0';
        return 0;
    }

    read_size = fread(buffer, 1, (size_t)buffer_size - 1, fp);
    buffer[read_size] = '\0';
    fclose(fp);
    return 1;
}