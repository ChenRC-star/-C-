#ifndef SERVER_H
#define SERVER_H

#include "http.h"

#define SERVER_DEFAULT_HOST "127.0.0.1"
#define SERVER_DEFAULT_PORT 8080
#define SERVER_RECV_BUFFER_SIZE 65536
#define SERVER_BACKLOG 16

/*
 * 路由处理回调。
 * server.c 负责解析请求和发送响应；真正的业务分发交给 router.c。
 */
typedef void (*ServerRouteHandler)(const HttpRequest *request, HttpResponse *response, void *userdata);

/* 服务器配置。 */
typedef struct ServerConfig {
    const char *host;                    /* 监听地址，课程设计建议 127.0.0.1 */
    int port;                            /* 监听端口，如 8080 */
    int verbose;                         /* 是否输出访问日志 */
    ServerRouteHandler handler;          /* 路由处理函数 */
    void *userdata;                      /* 传给路由的上下文，如 AppData* */
} ServerConfig;

void server_config_init(ServerConfig *config);
int server_start(const ServerConfig *config);
int server_start_simple(int port, ServerRouteHandler handler, void *userdata);

#endif
