#ifndef ROUTER_H
#define ROUTER_H

#include "http.h"
#include "storage.h"

/*
 * router 模块是 HTTP 服务器和业务模块之间的分发层。
 *
 * server.c 接收到浏览器请求后，会先用 http.c 解析为 HttpRequest，
 * 然后调用 router_handle()。router_handle() 根据请求路径选择具体业务：
 *
 *   POST /api/login         -> 用户登录
 *   POST /api/register      -> 用户注册
 *   GET  /api/books         -> 查询图书
 *   POST /api/books         -> 发布图书
 *   POST /api/books/buy     -> 购买图书
 *   POST /api/books/remove  -> 下架图书
 *   GET  /api/trades        -> 查询交易记录
 *   GET  /api/stats         -> 获取统计报表
 *
 * 非 /api/ 开头的路径会作为静态文件处理，例如 /index.html。
 */
#define ROUTER_WEB_ROOT "web"

/* server.c 调用的总入口。userdata 必须传入 AppData*。 */
void router_handle(const HttpRequest *request, HttpResponse *response, void *userdata);

#endif
