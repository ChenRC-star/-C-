#include "router.h"
#include "server.h"
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * main.c 是整个 Web 后端程序的入口。
 *
 * 启动流程：
 *   1. 初始化 AppData，设置 CSV 数据文件路径
 *   2. 从 data 目录中的 CSV 读取用户、图书、交易记录到动态链表
 *   3. 配置 Windows Socket HTTP 服务器
 *   4. 把 router_handle 作为路由回调交给 server.c
 *   5. 阻塞监听浏览器请求
 */

static void print_banner(void)
{
    printf("============================================================\n");
    printf(" 校园二手书交易管理系统 - Socket HTTP Server\n");
    printf(" Windows + C + Winsock + CSV + Web Page\n");
    printf("============================================================\n");
}

static void print_data_summary(const AppData *data)
{
    if (data == NULL) {
        return;
    }

    printf("数据加载完成：\n");
    printf("  用户数量：%d\n", user_count(data->users));
    printf("  图书数量：%d\n", book_count(data->books));
    printf("  交易数量：%d\n", trade_count(data->trades));
    printf("数据文件：\n");
    printf("  %s\n", data->users_file);
    printf("  %s\n", data->books_file);
    printf("  %s\n", data->trades_file);
    printf("------------------------------------------------------------\n");
}

int main(void)
{
    AppData data;
    ServerConfig config;

    print_banner();

    storage_init(&data);
    if (!storage_load_all(&data)) {
        printf("数据加载失败，请检查 data 目录下的 CSV 文件。\n");
        storage_free(&data);
        return 1;
    }

    print_data_summary(&data);

    server_config_init(&config);
    config.host = SERVER_DEFAULT_HOST;
    config.port = SERVER_DEFAULT_PORT;
    config.verbose = 1;
    config.handler = router_handle;
    config.userdata = &data;

    /*
     * server_start 是阻塞函数，会一直监听请求。
     * 用户按 Ctrl+C 结束程序时，操作系统会回收进程资源；
     * 正常业务修改已经在 storage_* 函数中即时保存到 CSV。
     */
    if (!server_start(&config)) {
        printf("HTTP 服务器启动失败。\n");
        storage_free(&data);
        return 1;
    }

    storage_free(&data);
    return 0;
}
