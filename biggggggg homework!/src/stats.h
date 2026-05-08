#ifndef STATS_H
#define STATS_H

#include "book.h"
#include "trade.h"

#include <stdio.h>

#define STATS_CATEGORY_NAME_LEN BOOK_CATEGORY_LEN
#define STATS_MAX_CATEGORIES    32

/* 单个分类的成交统计。 */
typedef struct CategoryStats {
    char category[STATS_CATEGORY_NAME_LEN];
    int listed_count;
    int sold_count;
    double sold_amount;
} CategoryStats;

/* 给 Web 仪表盘和报表使用的综合统计。 */
typedef struct SystemStats {
    BookStats book_stats;
    TradeStats trade_stats;
    int user_count;
    int category_count;
    CategoryStats categories[STATS_MAX_CATEGORIES];
} SystemStats;

void stats_init(SystemStats *stats);
SystemStats stats_calculate(const Book *books,
                            const Trade *trades,
                            int user_count,
                            const char *begin_time,
                            const char *end_time);
int stats_export_report(const char *filename, const SystemStats *stats);
void stats_print_report(FILE *out, const SystemStats *stats);

/* 生成简洁 JSON，便于 /api/stats 直接返回给前端 fetch 使用。 */
int stats_to_json(const SystemStats *stats, char *buffer, int buffer_size);

#endif
