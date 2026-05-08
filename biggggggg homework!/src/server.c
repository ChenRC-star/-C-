#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

/*
 * MinGW 使用 -lws2_32 链接；MSVC 可通过 pragma 自动链接。
 * 如果使用 gcc 编译完整项目，请命令末尾加：-lws2_32
 */
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

static void default_route_handler(const HttpRequest *request, HttpResponse *response, void *userdata)
{
    (void)request;
    (void)userdata;
    http_response_error(response, 404, "未找到对应路由");
}

void server_config_init(ServerConfig *config)
{
    if (config == NULL) {
        return;
    }

    config->host = SERVER_DEFAULT_HOST;
    config->port = SERVER_DEFAULT_PORT;
    config->verbose = 1;
    config->handler = default_route_handler;
    config->userdata = NULL;
}

static int send_all(SOCKET client_socket, const char *data, int length)
{
    int total_sent = 0;

    while (total_sent < length) {
        int sent = send(client_socket, data + total_sent, length - total_sent, 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            return 0;
        }
        total_sent += sent;
    }

    return 1;
}

static int create_listen_socket(const char *host, int port, SOCKET *listen_socket)
{
    SOCKET sock;
    struct sockaddr_in address;
    int reuse = 1;

    if (listen_socket == NULL) {
        return 0;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("socket 创建失败，错误码：%d\n", WSAGetLastError());
        return 0;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);

    if (host == NULL || strcmp(host, "0.0.0.0") == 0) {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        address.sin_addr.s_addr = inet_addr(host);
        if (address.sin_addr.s_addr == INADDR_NONE) {
            printf("监听地址无效：%s\n", host);
            closesocket(sock);
            return 0;
        }
    }

    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("bind 失败，端口可能被占用，错误码：%d\n", WSAGetLastError());
        closesocket(sock);
        return 0;
    }

    if (listen(sock, SERVER_BACKLOG) == SOCKET_ERROR) {
        printf("listen 失败，错误码：%d\n", WSAGetLastError());
        closesocket(sock);
        return 0;
    }

    *listen_socket = sock;
    return 1;
}

static int receive_request(SOCKET client_socket, char *buffer, int buffer_size)
{
    int received;

    if (buffer == NULL || buffer_size <= 0) {
        return -1;
    }

    received = recv(client_socket, buffer, buffer_size - 1, 0);
    if (received == SOCKET_ERROR) {
        return -1;
    }

    buffer[received] = '\0';
    return received;
}

static void handle_client(SOCKET client_socket, const ServerConfig *config)
{
    char request_buffer[SERVER_RECV_BUFFER_SIZE];
    int received;
    HttpRequest request;
    HttpResponse response;
    ServerRouteHandler handler;

    received = receive_request(client_socket, request_buffer, sizeof(request_buffer));
    if (received <= 0) {
        return;
    }

    if (!http_parse_request(request_buffer, received, &request)) {
        http_response_error(&response, 400, "HTTP 请求格式错误");
        send_all(client_socket, response.raw, response.raw_length);
        return;
    }

    if (config != NULL && config->verbose) {
        printf("%s %s%s%s\n",
               request.method,
               request.path,
               request.query[0] == '\0' ? "" : "?",
               request.query);
    }

    if (http_is_method(&request, "OPTIONS")) {
        http_response_text(&response, 204, "");
        send_all(client_socket, response.raw, response.raw_length);
        return;
    }

    http_response_init(&response);
    handler = config != NULL && config->handler != NULL ? config->handler : default_route_handler;
    handler(&request, &response, config == NULL ? NULL : config->userdata);

    if (response.raw_length <= 0) {
        http_build_response(&response);
    }

    send_all(client_socket, response.raw, response.raw_length);
}

int server_start(const ServerConfig *config)
{
    WSADATA wsa_data;
    SOCKET listen_socket = INVALID_SOCKET;
    ServerConfig local_config;

    if (config == NULL) {
        server_config_init(&local_config);
        config = &local_config;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("WSAStartup 失败\n");
        return 0;
    }

    if (!create_listen_socket(config->host, config->port, &listen_socket)) {
        WSACleanup();
        return 0;
    }

    printf("校园二手书交易管理系统 HTTP 服务器已启动\n");
    printf("访问地址：http://%s:%d\n", config->host, config->port);
    printf("按 Ctrl+C 停止服务器\n");

    while (1) {
        SOCKET client_socket;
        struct sockaddr_in client_address;
        int address_len = sizeof(client_address);

        client_socket = accept(listen_socket, (struct sockaddr *)&client_address, &address_len);
        if (client_socket == INVALID_SOCKET) {
            printf("accept 失败，错误码：%d\n", WSAGetLastError());
            continue;
        }

        handle_client(client_socket, config);
        shutdown(client_socket, SD_BOTH);
        closesocket(client_socket);
    }

    closesocket(listen_socket);
    WSACleanup();
    return 1;
}

int server_start_simple(int port, ServerRouteHandler handler, void *userdata)
{
    ServerConfig config;

    server_config_init(&config);
    config.port = port;
    config.handler = handler == NULL ? default_route_handler : handler;
    config.userdata = userdata;

    return server_start(&config);
}
