#include "stats.h"

#include "utils.h"

#include <stdio.h>
#include <string.h>

void stats_init(SystemStats *stats)
{
    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(SystemStats));
}

static int find_category(SystemStats *stats, const char *category)
{
    int i;

    if (stats == NULL || utils_is_blank(category)) {
        return -1;
    }

    for (i = 0; i < stats->category_count; ++i) {
        if (strcmp(stats->categories[i].category, category) == 0) {
            return i;
        }
    }

    return -1;
}

static int ensure_category(SystemStats *stats, const char *category)
{
    int index = find_category(stats, category);

    if (index >= 0) {
        return index;
    }
    if (stats == NULL || stats->category_count >= STATS_MAX_CATEGORIES) {
        return -1;
    }

    index = stats->category_count++;
    utils_safe_copy(stats->categories[index].category,
                    sizeof(stats->categories[index].category),
                    utils_is_blank(category) ? "未分类" : category);
    return index;
}

static int time_in_range(const char *time_text, const char *begin_time, const char *end_time)
{
    if (utils_is_blank(time_text)) {
        return 0;
    }
    if (!utils_is_blank(begin_time) && strcmp(time_text, begin_time) < 0) {
        return 0;
    }
    if (!utils_is_blank(end_time) && strcmp(time_text, end_time) > 0) {
        return 0;
    }
    return 1;
}

static void calculate_category_stats(SystemStats *stats,
                                     const Book *books,
                                     const char *begin_time,
                                     const char *end_time)
{
    while (books != NULL) {
        int index;

        if (time_in_range(books->publish_time, begin_time, end_time)) {
            index = ensure_category(stats, books->category);
            if (index >= 0) {
                ++stats->categories[index].listed_count;
            }
        }

        if (books->status == BOOK_STATUS_SOLD && time_in_range(books->trade_time, begin_time, end_time)) {
            index = ensure_category(stats, books->category);
            if (index >= 0) {
                ++stats->categories[index].sold_count;
                stats->categories[index].sold_amount += books->price;
            }
        }

        books = books->next;
    }
}

SystemStats stats_calculate(const Book *books,
                            const Trade *trades,
                            int user_count,
                            const char *begin_time,
                            const char *end_time)
{
    SystemStats stats;

    stats_init(&stats);
    stats.book_stats = book_calculate_stats(books, begin_time, end_time);
    stats.trade_stats = trade_calculate_stats(trades, begin_time, end_time);
    stats.user_count = user_count;
    calculate_category_stats(&stats, books, begin_time, end_time);

    return stats;
}

void stats_print_report(FILE *out, const SystemStats *stats)
{
    int i;

    if (out == NULL || stats == NULL) {
        return;
    }

    fprintf(out, "校园二手书交易管理系统统计报表\n");
    fprintf(out, "用户数量：%d\n", stats->user_count);
    fprintf(out, "图书总上架数量：%d\n", stats->book_stats.total_count);
    fprintf(out, "在售数量：%d\n", stats->book_stats.on_sale_count);
    fprintf(out, "已售数量：%d\n", stats->book_stats.sold_count);
    fprintf(out, "已下架数量：%d\n", stats->book_stats.removed_count);
    fprintf(out, "图书成交金额：%.2f\n", stats->book_stats.total_sold_amount);
    fprintf(out, "交易记录数量：%d\n", stats->trade_stats.total_count);
    fprintf(out, "已完成交易数量：%d\n", stats->trade_stats.completed_count);
    fprintf(out, "已取消交易数量：%d\n", stats->trade_stats.cancelled_count);
    fprintf(out, "交易记录成交金额：%.2f\n", stats->trade_stats.completed_amount);
    fprintf(out, "\n分类统计：\n");
    fprintf(out, "%-16s %-10s %-10s %-10s\n", "分类", "上架数", "成交数", "成交额");

    for (i = 0; i < stats->category_count; ++i) {
        fprintf(out, "%-16s %-10d %-10d %-10.2f\n",
                stats->categories[i].category,
                stats->categories[i].listed_count,
                stats->categories[i].sold_count,
                stats->categories[i].sold_amount);
    }
}

int stats_export_report(const char *filename, const SystemStats *stats)
{
    FILE *fp;

    if (utils_is_blank(filename) || stats == NULL) {
        return 0;
    }

    fp = fopen(filename, "w");
    if (fp == NULL) {
        return 0;
    }

    stats_print_report(fp, stats);
    fclose(fp);
    return 1;
}

int stats_to_json(const SystemStats *stats, char *buffer, int buffer_size)
{
    int i;
    int used;

    if (stats == NULL || buffer == NULL || buffer_size <= 0) {
        return 0;
    }

    used = snprintf(buffer, (size_t)buffer_size,
                    "{\"users\":%d,"
                    "\"books\":{\"total\":%d,\"on_sale\":%d,\"sold\":%d,\"removed\":%d,\"amount\":%.2f},"
                    "\"trades\":{\"total\":%d,\"completed\":%d,\"cancelled\":%d,\"amount\":%.2f},"
                    "\"categories\":[",
                    stats->user_count,
                    stats->book_stats.total_count,
                    stats->book_stats.on_sale_count,
                    stats->book_stats.sold_count,
                    stats->book_stats.removed_count,
                    stats->book_stats.total_sold_amount,
                    stats->trade_stats.total_count,
                    stats->trade_stats.completed_count,
                    stats->trade_stats.cancelled_count,
                    stats->trade_stats.completed_amount);

    if (used < 0 || used >= buffer_size) {
        return 0;
    }

    for (i = 0; i < stats->category_count; ++i) {
        char escaped[STATS_CATEGORY_NAME_LEN * 2];
        int written;

        utils_json_escape(stats->categories[i].category, escaped, sizeof(escaped));
        written = snprintf(buffer + used, (size_t)(buffer_size - used),
                           "%s{\"category\":\"%s\",\"listed\":%d,\"sold\":%d,\"amount\":%.2f}",
                           i == 0 ? "" : ",",
                           escaped,
                           stats->categories[i].listed_count,
                           stats->categories[i].sold_count,
                           stats->categories[i].sold_amount);
        if (written < 0 || written >= buffer_size - used) {
            return 0;
        }
        used += written;
    }

    if (snprintf(buffer + used, (size_t)(buffer_size - used), "]}") >= buffer_size - used) {
        return 0;
    }

    return 1;
}
