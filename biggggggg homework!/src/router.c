#include "router.h"

#include "stats.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/*
 * router.c 负责“路径 -> 业务函数”的分发和 JSON 组装。
 *
 * 它不直接操作 socket，也不直接解析原始 HTTP 报文：
 *   - socket 收发由 server.c 完成
 *   - HTTP 解析和响应封装由 http.c 完成
 *   - 数据读写和组合业务由 storage.c 完成
 */

/* 判断当前请求路径是否等于指定 path。 */
static int path_equals(const HttpRequest *request, const char *path)
{
    return request != NULL && path != NULL && strcmp(request->path, path) == 0;
}

/* 判断 text 是否以 prefix 开头，用于识别 /api/ 这类路径前缀。 */
static int starts_with(const char *text, const char *prefix)
{
    size_t len;

    if (text == NULL || prefix == NULL) {
        return 0;
    }

    len = strlen(prefix);
    return strncmp(text, prefix, len) == 0;
}

/* 返回统一格式的成功 JSON：{"success":true,"message":"..."}。 */
static void json_success(HttpResponse *response, const char *message)
{
    char escaped[1024];
    char json[1400];

    utils_json_escape(message == NULL ? "操作成功" : message, escaped, sizeof(escaped));
    snprintf(json, sizeof(json), "{\"success\":true,\"message\":\"%s\"}", escaped);
    http_response_json(response, 200, json);
}

/* 返回统一格式的失败 JSON，具体格式由 http_response_error 封装。 */
static void json_fail(HttpResponse *response, int status_code, const char *message)
{
    http_response_error(response, status_code, message);
}

/* 向 JSON 缓冲区追加普通字符串，并维护 used 偏移量。 */
static void append_text(char *buffer, int buffer_size, int *used, const char *text)
{
    int written;

    if (buffer == NULL || used == NULL || text == NULL || *used >= buffer_size) {
        return;
    }

    written = snprintf(buffer + *used, (size_t)(buffer_size - *used), "%s", text);
    if (written < 0) {
        return;
    }
    if (written >= buffer_size - *used) {
        *used = buffer_size - 1;
    } else {
        *used += written;
    }
}

/* 向 JSON 缓冲区追加格式化内容，避免多次手写 snprintf 偏移计算。 */
static void append_format(char *buffer, int buffer_size, int *used, const char *format, ...)
{
    va_list args;
    int written;

    if (buffer == NULL || used == NULL || format == NULL || *used >= buffer_size) {
        return;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *used, (size_t)(buffer_size - *used), format, args);
    va_end(args);

    if (written < 0) {
        return;
    }
    if (written >= buffer_size - *used) {
        *used = buffer_size - 1;
    } else {
        *used += written;
    }
}

/*
 * POST /api/login
 *
 * 表单参数：
 *   student_id  学号
 *   password    密码
 *
 * 成功后会校验密码、创建会话 token、更新最近登录时间，
 * 并返回 token、学号、姓名和角色给前端。
 */
static void handle_login(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    char student_id[USER_STUDENT_ID_LEN];
    char password[USER_PASSWORD_LEN];
    User *user;
    UserSession *session;
    char json[2048];
    char name[USER_NAME_LEN * 2];

    if (!http_is_method(request, "POST")) {
        json_fail(response, 405, "登录接口只支持 POST");
        return;
    }

    http_get_form_value(request, "student_id", student_id, sizeof(student_id));
    http_get_form_value(request, "password", password, sizeof(password));

    user = user_login(data->users, student_id, password);
    if (user == NULL) {
        json_fail(response, 401, "学号或密码错误");
        return;
    }

    session = session_create(user->student_id);
    if (session == NULL || !session_insert(&data->sessions, session)) {
        if (session != NULL) {
            free(session);
        }
        json_fail(response, 500, "创建登录会话失败");
        return;
    }

    storage_save_users(data);
    utils_json_escape(user->name, name, sizeof(name));
    snprintf(json, sizeof(json),
             "{\"success\":true,\"message\":\"登录成功\",\"token\":\"%s\",\"student_id\":\"%s\",\"name\":\"%s\",\"role\":\"%s\"}",
             session->token,
             user->student_id,
             name,
             user_role_to_string(user->role));
    http_response_json(response, 200, json);
}

/*
 * POST /api/register
 *
 * 表单参数：
 *   student_id, name, password, phone, email
 *
 * 注册成功后会立即保存 users.csv。
 */
static void handle_register(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    char student_id[USER_STUDENT_ID_LEN];
    char name[USER_NAME_LEN];
    char password[USER_PASSWORD_LEN];
    char phone[USER_PHONE_LEN];
    char email[USER_EMAIL_LEN];

    if (!http_is_method(request, "POST")) {
        json_fail(response, 405, "注册接口只支持 POST");
        return;
    }

    http_get_form_value(request, "student_id", student_id, sizeof(student_id));
    http_get_form_value(request, "name", name, sizeof(name));
    http_get_form_value(request, "password", password, sizeof(password));
    http_get_form_value(request, "phone", phone, sizeof(phone));
    http_get_form_value(request, "email", email, sizeof(email));

    if (!storage_register_user(data, student_id, name, password, phone, email)) {
        json_fail(response, 400, "注册失败：请检查学号是否重复、姓名是否为空、密码是否至少 6 位");
        return;
    }

    json_success(response, "注册成功");
}

/*
 * 把 GET /api/books 的查询参数转换为 BookQuery。
 *
 * 支持参数：
 *   keyword, isbn, author, publisher, category, seller_id,
 *   status, min_price, max_price
 */
static void query_books_from_request(const HttpRequest *request, BookQuery *query)
{
    char value[128];

    book_init_query(query);
    http_get_query_param(request, "keyword", query->keyword, sizeof(query->keyword));
    http_get_query_param(request, "isbn", query->isbn, sizeof(query->isbn));
    http_get_query_param(request, "author", query->author, sizeof(query->author));
    http_get_query_param(request, "publisher", query->publisher, sizeof(query->publisher));
    http_get_query_param(request, "category", query->category, sizeof(query->category));
    http_get_query_param(request, "seller_id", query->seller_id, sizeof(query->seller_id));

    if (http_get_query_param(request, "status", value, sizeof(value))) {
        query->status = book_status_from_string(value);
        query->use_status = 1;
    }
    if (http_get_query_param(request, "min_price", value, sizeof(value))) {
        query->min_price = atof(value);
    }
    if (http_get_query_param(request, "max_price", value, sizeof(value))) {
        query->max_price = atof(value);
    }
}

/* 把一个 Book 节点追加为 JSON 对象；字符串字段先转义，避免引号破坏 JSON。 */
static void append_book_json(char *json, int json_size, int *used, const Book *book, int comma)
{
    char title[256];
    char isbn[80];
    char author[128];
    char publisher[128];
    char category[128];
    char condition[80];
    char seller_name[128];
    char seller_contact[128];

    utils_json_escape(book->title, title, sizeof(title));
    utils_json_escape(book->isbn, isbn, sizeof(isbn));
    utils_json_escape(book->author, author, sizeof(author));
    utils_json_escape(book->publisher, publisher, sizeof(publisher));
    utils_json_escape(book->category, category, sizeof(category));
    utils_json_escape(book->condition, condition, sizeof(condition));
    utils_json_escape(book->seller_name, seller_name, sizeof(seller_name));
    utils_json_escape(book->seller_contact, seller_contact, sizeof(seller_contact));

    append_format(json, json_size, used,
                  "%s{\"id\":%d,\"title\":\"%s\",\"isbn\":\"%s\",\"author\":\"%s\","
                  "\"publisher\":\"%s\",\"category\":\"%s\",\"condition\":\"%s\","
                  "\"price\":%.2f,\"seller_id\":\"%s\",\"seller_name\":\"%s\","
                  "\"seller_contact\":\"%s\",\"buyer_id\":\"%s\",\"status\":\"%s\","
                  "\"publish_time\":\"%s\",\"trade_time\":\"%s\"}",
                  comma ? "," : "",
                  book->id,
                  title,
                  isbn,
                  author,
                  publisher,
                  category,
                  condition,
                  book->price,
                  book->seller_id,
                  seller_name,
                  seller_contact,
                  book->buyer_id,
                  book_status_to_string(book->status),
                  book->publish_time,
                  book->trade_time);
}

/*
 * GET /api/books
 *
 * 根据 query 参数过滤图书链表，返回：
 *   {"success":true,"books":[...],"count":N}
 */
static void handle_books_get(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    BookQuery query;
    const Book *book;
    char json[HTTP_RESPONSE_LEN];
    int used = 0;
    int count = 0;

    query_books_from_request(request, &query);
    append_text(json, sizeof(json), &used, "{\"success\":true,\"books\":[");

    for (book = data->books; book != NULL; book = book->next) {
        if (book_matches_query(book, &query)) {
            append_book_json(json, sizeof(json), &used, book, count > 0);
            ++count;
        }
    }

    append_format(json, sizeof(json), &used, "],\"count\":%d}", count);
    http_response_json(response, 200, json);
}

/*
 * POST /api/books
 *
 * 发布图书。表单参数：
 *   title, isbn, author, publisher, category, condition, price,
 *   seller_id, seller_name, seller_contact
 *
 * 成功后会保存 books.csv。
 */
static void handle_books_post(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    char title[BOOK_TITLE_LEN];
    char isbn[BOOK_ISBN_LEN];
    char author[BOOK_AUTHOR_LEN];
    char publisher[BOOK_PUBLISHER_LEN];
    char category[BOOK_CATEGORY_LEN];
    char condition[BOOK_CONDITION_LEN];
    char price_text[64];
    char seller_id[BOOK_STUDENT_ID_LEN];
    char seller_name[BOOK_NAME_LEN];
    char seller_contact[BOOK_CONTACT_LEN];
    double price;

    http_get_form_value(request, "title", title, sizeof(title));
    http_get_form_value(request, "isbn", isbn, sizeof(isbn));
    http_get_form_value(request, "author", author, sizeof(author));
    http_get_form_value(request, "publisher", publisher, sizeof(publisher));
    http_get_form_value(request, "category", category, sizeof(category));
    http_get_form_value(request, "condition", condition, sizeof(condition));
    http_get_form_value(request, "price", price_text, sizeof(price_text));
    http_get_form_value(request, "seller_id", seller_id, sizeof(seller_id));
    http_get_form_value(request, "seller_name", seller_name, sizeof(seller_name));
    http_get_form_value(request, "seller_contact", seller_contact, sizeof(seller_contact));

    price = atof(price_text);
    if (!storage_publish_book(data, title, isbn, author, publisher, category, condition,
                              price, seller_id, seller_name, seller_contact)) {
        json_fail(response, 400, "发布失败：请检查书名、卖家学号和价格");
        return;
    }

    json_success(response, "发布成功");
}

/* /api/books 的统一入口：GET 查询图书，POST 发布图书。 */
static void handle_books(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    if (http_is_method(request, "GET")) {
        handle_books_get(request, response, data);
    } else if (http_is_method(request, "POST")) {
        handle_books_post(request, response, data);
    } else {
        json_fail(response, 405, "图书接口只支持 GET 或 POST");
    }
}

/*
 * POST /api/books/buy
 *
 * 表单参数：
 *   book_id   图书编号
 *   buyer_id  买家学号
 *
 * storage_buy_book 会同时完成：
 *   1. 修改图书状态为已售
 *   2. 记录买家和成交时间
 *   3. 新增交易记录
 *   4. 保存 books.csv 和 trades.csv
 */
static void handle_buy_book(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    char book_id_text[32];
    char buyer_id[BOOK_STUDENT_ID_LEN];
    int book_id;

    if (!http_is_method(request, "POST")) {
        json_fail(response, 405, "购买接口只支持 POST");
        return;
    }

    http_get_form_value(request, "book_id", book_id_text, sizeof(book_id_text));
    http_get_form_value(request, "buyer_id", buyer_id, sizeof(buyer_id));
    book_id = atoi(book_id_text);

    if (!storage_buy_book(data, book_id, buyer_id)) {
        json_fail(response, 400, "购买失败：图书不存在、非在售状态，或不能购买自己发布的书");
        return;
    }

    json_success(response, "购买成功");
}

/*
 * POST /api/books/remove
 *
 * 表单参数：
 *   book_id    图书编号
 *   seller_id  卖家学号
 *
 * 只有图书仍在售，且 seller_id 与发布者一致时才允许下架。
 */
static void handle_remove_book(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    char book_id_text[32];
    char seller_id[BOOK_STUDENT_ID_LEN];
    int book_id;

    if (!http_is_method(request, "POST")) {
        json_fail(response, 405, "下架接口只支持 POST");
        return;
    }

    http_get_form_value(request, "book_id", book_id_text, sizeof(book_id_text));
    http_get_form_value(request, "seller_id", seller_id, sizeof(seller_id));
    book_id = atoi(book_id_text);

    if (!storage_take_down_book(data, book_id, seller_id)) {
        json_fail(response, 400, "下架失败：图书不存在、非在售状态，或卖家身份不匹配");
        return;
    }

    json_success(response, "下架成功");
}

/*
 * 把 GET /api/trades 的查询参数转换为 TradeQuery。
 *
 * 支持参数：
 *   trade_id, book_id, buyer_id, seller_id, begin_time, end_time, status
 */
static void query_trades_from_request(const HttpRequest *request, TradeQuery *query)
{
    char value[128];

    trade_init_query(query);
    if (http_get_query_param(request, "trade_id", value, sizeof(value))) {
        query->trade_id = atoi(value);
    }
    if (http_get_query_param(request, "book_id", value, sizeof(value))) {
        query->book_id = atoi(value);
    }
    http_get_query_param(request, "buyer_id", query->buyer_id, sizeof(query->buyer_id));
    http_get_query_param(request, "seller_id", query->seller_id, sizeof(query->seller_id));
    http_get_query_param(request, "begin_time", query->begin_time, sizeof(query->begin_time));
    http_get_query_param(request, "end_time", query->end_time, sizeof(query->end_time));
    if (http_get_query_param(request, "status", value, sizeof(value))) {
        query->status = trade_status_from_string(value);
        query->use_status = 1;
    }
}

/*
 * GET /api/trades
 *
 * 查询交易记录，返回：
 *   {"success":true,"trades":[...],"count":N}
 */
static void handle_trades_get(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    TradeQuery query;
    const Trade *trade;
    char json[HTTP_RESPONSE_LEN];
    int used = 0;
    int count = 0;

    query_trades_from_request(request, &query);
    append_text(json, sizeof(json), &used, "{\"success\":true,\"trades\":[");

    for (trade = data->trades; trade != NULL; trade = trade->next) {
        if (trade_matches_query(trade, &query)) {
            append_format(json, sizeof(json), &used,
                          "%s{\"trade_id\":%d,\"book_id\":%d,\"buyer_id\":\"%s\","
                          "\"seller_id\":\"%s\",\"price\":%.2f,\"trade_time\":\"%s\",\"status\":\"%s\"}",
                          count > 0 ? "," : "",
                          trade->trade_id,
                          trade->book_id,
                          trade->buyer_id,
                          trade->seller_id,
                          trade->price,
                          trade->trade_time,
                          trade_status_to_string(trade->status));
            ++count;
        }
    }

    append_format(json, sizeof(json), &used, "],\"count\":%d}", count);
    http_response_json(response, 200, json);
}

/*
 * GET /api/stats
 *
 * 可选 query 参数：
 *   begin_time  起始时间，格式 YYYY-MM-DD HH:MM:SS
 *   end_time    结束时间，格式 YYYY-MM-DD HH:MM:SS
 *
 * 返回系统综合统计 JSON，包括用户数、图书状态数量、交易金额、分类统计。
 */
static void handle_stats(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    char begin_time[BOOK_TIME_LEN] = "";
    char end_time[BOOK_TIME_LEN] = "";
    char json[HTTP_RESPONSE_LEN];
    SystemStats stats;

    if (!http_is_method(request, "GET")) {
        json_fail(response, 405, "统计接口只支持 GET");
        return;
    }

    http_get_query_param(request, "begin_time", begin_time, sizeof(begin_time));
    http_get_query_param(request, "end_time", end_time, sizeof(end_time));
    stats = stats_calculate(data->books, data->trades, user_count(data->users), begin_time, end_time);

    if (!stats_to_json(&stats, json, sizeof(json))) {
        json_fail(response, 500, "统计结果生成失败");
        return;
    }

    http_response_json(response, 200, json);
}

/*
 * GET /api/me
 *
 * query 参数：
 *   token  登录时返回的会话令牌
 *
 * 用 token 找到当前用户，返回基本资料。前端可用于刷新页面后恢复登录状态。
 */
static void handle_me(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    char token[USER_TOKEN_LEN];
    const User *user;
    char json[2048];
    char name[USER_NAME_LEN * 2];

    if (!http_is_method(request, "GET")) {
        json_fail(response, 405, "当前用户接口只支持 GET");
        return;
    }

    http_get_query_param(request, "token", token, sizeof(token));
    user = session_get_user(data->sessions, data->users, token);
    if (user == NULL) {
        json_fail(response, 401, "未登录或会话已失效");
        return;
    }

    utils_json_escape(user->name, name, sizeof(name));
    snprintf(json, sizeof(json),
             "{\"success\":true,\"student_id\":\"%s\",\"name\":\"%s\",\"phone\":\"%s\",\"email\":\"%s\",\"role\":\"%s\"}",
             user->student_id, name, user->phone, user->email, user_role_to_string(user->role));
    http_response_json(response, 200, json);
}

/*
 * POST /api/logout
 *
 * 表单参数：
 *   token
 *
 * 从会话链表中删除 token。当前会话只保存在内存中，重启服务器后自动失效。
 */
static void handle_logout(const HttpRequest *request, HttpResponse *response, AppData *data)
{
    char token[USER_TOKEN_LEN];

    if (!http_is_method(request, "POST")) {
        json_fail(response, 405, "退出接口只支持 POST");
        return;
    }

    http_get_form_value(request, "token", token, sizeof(token));
    session_remove_by_token(&data->sessions, token);
    json_success(response, "已退出登录");
}

/* 静态文件路径安全检查：禁止 .. 和 Windows 盘符路径，防止读出 web 目录外文件。 */
static int is_safe_static_path(const char *path)
{
    return path != NULL && strstr(path, "..") == NULL && strchr(path, ':') == NULL;
}

/*
 * 处理静态页面和资源：
 *   /              -> web/index.html
 *   /login.html    -> web/login.html
 *   /css/style.css -> web/css/style.css
 */
static void handle_static_file(const HttpRequest *request, HttpResponse *response)
{
    char file_path[512];
    const char *relative;

    if (!http_is_method(request, "GET")) {
        http_response_error(response, 405, "静态文件只支持 GET");
        return;
    }

    if (strcmp(request->path, "/") == 0) {
        relative = "index.html";
    } else {
        relative = request->path[0] == '/' ? request->path + 1 : request->path;
    }

    if (!is_safe_static_path(relative)) {
        http_response_error(response, 403, "非法文件路径");
        return;
    }

    utils_join_path(file_path, sizeof(file_path), ROUTER_WEB_ROOT, relative);
    http_response_file(response, file_path);
}

/*
 * 路由总入口。
 *
 * server.c 会把每个请求传到这里。这里按 request->path 顺序匹配：
 *   1. 精确匹配各个 /api/... 接口
 *   2. /api/ 开头但未匹配的返回 404
 *   3. 其他路径当作静态文件返回
 */
void router_handle(const HttpRequest *request, HttpResponse *response, void *userdata)
{
    AppData *data = (AppData *)userdata;

    if (request == NULL || response == NULL) {
        return;
    }
    if (data == NULL) {
        http_response_error(response, 500, "服务器数据未初始化");
        return;
    }

    if (path_equals(request, "/api/login")) {
        handle_login(request, response, data);
    } else if (path_equals(request, "/api/register")) {
        handle_register(request, response, data);
    } else if (path_equals(request, "/api/me")) {
        handle_me(request, response, data);
    } else if (path_equals(request, "/api/logout")) {
        handle_logout(request, response, data);
    } else if (path_equals(request, "/api/books")) {
        handle_books(request, response, data);
    } else if (path_equals(request, "/api/books/buy")) {
        handle_buy_book(request, response, data);
    } else if (path_equals(request, "/api/books/remove")) {
        handle_remove_book(request, response, data);
    } else if (path_equals(request, "/api/trades")) {
        handle_trades_get(request, response, data);
    } else if (path_equals(request, "/api/stats")) {
        handle_stats(request, response, data);
    } else if (starts_with(request->path, "/api/")) {
        http_response_error(response, 404, "API 不存在");
    } else {
        handle_static_file(request, response);
    }
}
