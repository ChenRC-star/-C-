#ifndef TRADE_H
#define TRADE_H

#include "book.h"

#include <stdio.h>

/* 交易记录字段长度，和 book/user 模块保持一致。 */
#define TRADE_STUDENT_ID_LEN BOOK_STUDENT_ID_LEN
#define TRADE_TIME_LEN       BOOK_TIME_LEN

/* 交易状态。当前系统主要使用已完成，预留已取消方便后续扩展退款/撤销。 */
typedef enum TradeStatus {
    TRADE_STATUS_COMPLETED = 0,
    TRADE_STATUS_CANCELLED = 1
} TradeStatus;

/* 单条交易记录节点。 */
typedef struct Trade {
    int trade_id;                                  /* 交易编号，系统内唯一 */
    int book_id;                                   /* 对应图书编号 */
    char buyer_id[TRADE_STUDENT_ID_LEN];           /* 买家学号 */
    char seller_id[TRADE_STUDENT_ID_LEN];          /* 卖家学号 */
    double price;                                  /* 成交价格 */
    char trade_time[TRADE_TIME_LEN];               /* 成交时间 */
    TradeStatus status;                            /* 交易状态 */
    struct Trade *next;                            /* 下一交易节点 */
} Trade;

/* 交易查询条件。字符串为空表示不限制，编号小于等于 0 表示不限制。 */
typedef struct TradeQuery {
    int trade_id;                                  /* 交易编号 */
    int book_id;                                   /* 图书编号 */
    char buyer_id[TRADE_STUDENT_ID_LEN];           /* 买家学号 */
    char seller_id[TRADE_STUDENT_ID_LEN];          /* 卖家学号 */
    char begin_time[TRADE_TIME_LEN];               /* 开始时间 */
    char end_time[TRADE_TIME_LEN];                 /* 结束时间 */
    TradeStatus status;                            /* 指定状态 */
    int use_status;                                /* 是否启用状态筛选 */
    double min_price;                              /* 最低成交价，负数表示不限 */
    double max_price;                              /* 最高成交价，负数表示不限 */
} TradeQuery;

/* 交易统计结果。 */
typedef struct TradeStats {
    int total_count;                               /* 总交易记录数 */
    int completed_count;                           /* 已完成数量 */
    int cancelled_count;                           /* 已取消数量 */
    double completed_amount;                       /* 已完成交易金额 */
} TradeStats;

/* 遍历交易记录时使用的回调函数。 */
typedef void (*TradeVisitFunc)(const Trade *trade, void *userdata);

/* 基础工具函数 */
void trade_init_query(TradeQuery *query);
const char *trade_status_to_string(TradeStatus status);
TradeStatus trade_status_from_string(const char *text);
void trade_get_current_time(char *buffer, int size);

/* 链表生命周期 */
Trade *trade_create(int trade_id,
                    int book_id,
                    const char *buyer_id,
                    const char *seller_id,
                    double price,
                    const char *trade_time,
                    TradeStatus status);
void trade_free_all(Trade *head);
int trade_count(const Trade *head);
int trade_next_id(const Trade *head);

/* 增删改查 */
int trade_insert(Trade **head, Trade *trade);
Trade *trade_find_by_id(Trade *head, int trade_id);
const Trade *trade_find_by_id_const(const Trade *head, int trade_id);
int trade_delete_by_id(Trade **head, int trade_id);
int trade_add_completed(Trade **head,
                        int book_id,
                        const char *buyer_id,
                        const char *seller_id,
                        double price,
                        const char *trade_time);
int trade_add_from_book(Trade **head, const Book *book);
int trade_cancel(Trade *head, int trade_id);

/* 查询、统计、输出 */
int trade_matches_query(const Trade *trade, const TradeQuery *query);
int trade_query_each(const Trade *head, const TradeQuery *query, TradeVisitFunc visitor, void *userdata);
TradeStats trade_calculate_stats(const Trade *head, const char *begin_time, const char *end_time);
double trade_sum_amount_by_buyer(const Trade *head, const char *buyer_id);
double trade_sum_amount_by_seller(const Trade *head, const char *seller_id);
void trade_print_table_header(FILE *out);
void trade_print_table_row(FILE *out, const Trade *trade);
void trade_print_table(FILE *out, const Trade *head, const TradeQuery *query);
int trade_export_stats(const char *filename, const TradeStats *stats);

/* CSV 文件持久化 */
int trade_load_from_csv(const char *filename, Trade **head);
int trade_save_to_csv(const char *filename, const Trade *head);

#endif
