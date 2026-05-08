#include "trade.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TRADE_LINE_BUFFER 1024
#define TRADE_FIELD_COUNT 7

/* ================= 内部工具函数 (Static) ================= */

/**
 * @brief 安全复制字符串，确保不越界并以 '\0' 结尾。
 */
static void copy_text(char *dest, int dest_size, const char *src)
{
    if (dest == NULL || dest_size <= 0) return;
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, (size_t)dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/**
 * @brief 判断字符串是否为空。
 */
static int is_blank(const char *text)
{
    return text == NULL || text[0] == '\0';
}

/**
 * @brief 检查时间是否在指定区间内。
 * 由于时间格式为 YYYY-MM-DD HH:MM:SS，可以直接通过 strcmp 进行字典序比较。
 * @param begin_time 为空则不限制开始时间
 * @param end_time 为空则不限制结束时间
 */
static int time_in_range(const char *time_text, const char *begin_time, const char *end_time)
{
    if (is_blank(time_text)) return 0;
    if (!is_blank(begin_time) && strcmp(time_text, begin_time) < 0) return 0;
    if (!is_blank(end_time) && strcmp(time_text, end_time) > 0) return 0;
    return 1;
}

/* ================= 基础工具接口 ================= */

/**
 * @brief 初始化交易查询条件。
 * 默认不限制 ID、不限制状态、价格区间设为负数（表示不限）。
 */
void trade_init_query(TradeQuery *query)
{
    if (query == NULL) return;
    memset(query, 0, sizeof(TradeQuery));
    query->trade_id = 0;
    query->book_id = 0;
    query->status = TRADE_STATUS_COMPLETED;
    query->use_status = 0;
    query->min_price = -1.0;
    query->max_price = -1.0;
}

/**
 * @brief 交易状态枚举转中文。
 */
const char *trade_status_to_string(TradeStatus status)
{
    switch (status) {
    case TRADE_STATUS_CANCELLED: return "已取消";
    case TRADE_STATUS_COMPLETED: return "已完成";
    default:                     return "已完成";
    }
}

/**
 * @brief 从字符串解析交易状态（支持 CSV 数字标识或中文）。
 */
TradeStatus trade_status_from_string(const char *text)
{
    if (text == NULL) return TRADE_STATUS_COMPLETED;
    if (strcmp(text, "1") == 0 || strcmp(text, "已取消") == 0 || strcmp(text, "cancelled") == 0) {
        return TRADE_STATUS_CANCELLED;
    }
    return TRADE_STATUS_COMPLETED;
}

/**
 * @brief 获取格式化的当前系统时间。
 */
void trade_get_current_time(char *buffer, int size)
{
    time_t now;
    struct tm *local_time;
    if (buffer == NULL || size <= 0) return;

    now = time(NULL);
    local_time = localtime(&now);
    if (local_time == NULL) {
        copy_text(buffer, size, "");
        return;
    }
    strftime(buffer, (size_t)size, "%Y-%m-%d %H:%M:%S", local_time);
}

/* ================= 链表生命周期与维护 ================= */

/**
 * @brief 创建交易流水节点。
 * @param trade_time 若为空则自动填入当前系统时间。
 */
Trade *trade_create(int trade_id, int book_id, const char *buyer_id,
                    const char *seller_id, double price, const char *trade_time,
                    TradeStatus status)
{
    Trade *trade;
    if (trade_id <= 0 || book_id <= 0 || is_blank(buyer_id) || is_blank(seller_id) || price < 0.0) {
        return NULL;
    }

    trade = (Trade *)malloc(sizeof(Trade));
    if (trade == NULL) return NULL;

    memset(trade, 0, sizeof(Trade));
    trade->trade_id = trade_id;
    trade->book_id = book_id;
    copy_text(trade->buyer_id, TRADE_STUDENT_ID_LEN, buyer_id);
    copy_text(trade->seller_id, TRADE_STUDENT_ID_LEN, seller_id);
    trade->price = price;
    trade->status = status;

    if (is_blank(trade_time)) trade_get_current_time(trade->trade_time, TRADE_TIME_LEN);
    else copy_text(trade->trade_time, TRADE_TIME_LEN, trade_time);

    trade->next = NULL;
    return trade;
}

/**
 * @brief 释放交易链表内存。
 */
void trade_free_all(Trade *head)
{
    Trade *current = head;
    while (current != NULL) {
        Trade *next = current->next;
        free(current);
        current = next;
    }
}

/**
 * @brief 获取交易记录总条数。
 */
int trade_count(const Trade *head)
{
    int count = 0;
    while (head != NULL) {
        ++count;
        head = head->next;
    }
    return count;
}

/**
 * @brief 扫描链表获取下一个可用的交易 ID（自增逻辑）。
 */
int trade_next_id(const Trade *head)
{
    int max_id = 0;
    while (head != NULL) {
        if (head->trade_id > max_id) max_id = head->trade_id;
        head = head->next;
    }
    return max_id + 1;
}

/**
 * @brief 插入交易节点到链表尾部。
 */
int trade_insert(Trade **head, Trade *trade)
{
    Trade *tail;
    if (head == NULL || trade == NULL) return 0;
    if (trade_find_by_id(*head, trade->trade_id) != NULL) return 0;

    trade->next = NULL;
    if (*head == NULL) {
        *head = trade;
        return 1;
    }
    tail = *head;
    while (tail->next != NULL) tail = tail->next;
    tail->next = trade;
    return 1;
}

/* ================= 查询与业务逻辑 ================= */

/** @brief 按 ID 查找交易记录 */
Trade *trade_find_by_id(Trade *head, int trade_id)
{
    while (head != NULL) {
        if (head->trade_id == trade_id) return head;
        head = head->next;
    }
    return NULL;
}

/** @brief 按 ID 查找交易记录（只读） */
const Trade *trade_find_by_id_const(const Trade *head, int trade_id)
{
    while (head != NULL) {
        if (head->trade_id == trade_id) return head;
        head = head->next;
    }
    return NULL;
}

/** @brief 按 ID 逻辑或物理删除交易记录 */
int trade_delete_by_id(Trade **head, int trade_id)
{
    Trade *current, *previous = NULL;
    if (head == NULL) return 0;

    current = *head;
    while (current != NULL) {
        if (current->trade_id == trade_id) {
            if (previous == NULL) *head = current->next;
            else previous->next = current->next;
            free(current);
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return 0;
}

/**
 * @brief 新增一条“已完成”的交易记录。
 */
int trade_add_completed(Trade **head, int book_id, const char *buyer_id,
                        const char *seller_id, double price, const char *trade_time)
{
    Trade *trade;
    int trade_id;
    if (head == NULL) return 0;

    trade_id = trade_next_id(*head);
    trade = trade_create(trade_id, book_id, buyer_id, seller_id, price, trade_time, TRADE_STATUS_COMPLETED);
    if (trade == NULL) return 0;

    return trade_insert(head, trade);
}

/**
 * @brief 快捷接口：根据 Book 对象的当前信息生成交易记录。
 * 通常在用户确认购买、Book 状态变为 SOLD 后调用。
 */
int trade_add_from_book(Trade **head, const Book *book)
{
    if (book == NULL || book->status != BOOK_STATUS_SOLD || is_blank(book->buyer_id)) {
        return 0;
    }
    return trade_add_completed(head, book->id, book->buyer_id, book->seller_id, book->price, book->trade_time);
}

/** @brief 取消某笔交易（状态更新） */
int trade_cancel(Trade *head, int trade_id)
{
    Trade *trade = trade_find_by_id(head, trade_id);
    if (trade == NULL || trade->status == TRADE_STATUS_CANCELLED) return 0;
    trade->status = TRADE_STATUS_CANCELLED;
    return 1;
}

/**
 * @brief 核心过滤算法：检查交易记录是否匹配多个查询条件。
 */
int trade_matches_query(const Trade *trade, const TradeQuery *query)
{
    if (trade == NULL) return 0;
    if (query == NULL) return 1;

    if (query->trade_id > 0 && trade->trade_id != query->trade_id) return 0;
    if (query->book_id > 0 && trade->book_id != query->book_id) return 0;
    if (!is_blank(query->buyer_id) && strcmp(trade->buyer_id, query->buyer_id) != 0) return 0;
    if (!is_blank(query->seller_id) && strcmp(trade->seller_id, query->seller_id) != 0) return 0;
    if (!time_in_range(trade->trade_time, query->begin_time, query->end_time)) return 0;
    if (query->use_status && trade->status != query->status) return 0;
    if (query->min_price >= 0.0 && trade->price < query->min_price) return 0;
    if (query->max_price >= 0.0 && trade->price > query->max_price) return 0;

    return 1;
}

/**
 * @brief 遍历查询接口：对匹配的交易执行回调。
 */
int trade_query_each(const Trade *head, const TradeQuery *query, TradeVisitFunc visitor, void *userdata)
{
    int count = 0;
    while (head != NULL) {
        if (trade_matches_query(head, query)) {
            ++count;
            if (visitor != NULL) visitor(head, userdata);
        }
        head = head->next;
    }
    return count;
}

/* ================= 统计与报表 ================= */

/**
 * @brief 计算指定时间段内的统计数据。
 */
TradeStats trade_calculate_stats(const Trade *head, const char *begin_time, const char *end_time)
{
    TradeStats stats;
    memset(&stats, 0, sizeof(TradeStats));

    while (head != NULL) {
        if (time_in_range(head->trade_time, begin_time, end_time)) {
            ++stats.total_count;
            if (head->status == TRADE_STATUS_COMPLETED) {
                ++stats.completed_count;
                stats.completed_amount += head->price;
            } else if (head->status == TRADE_STATUS_CANCELLED) {
                ++stats.cancelled_count;
            }
        }
        head = head->next;
    }
    return stats;
}

/** @brief 统计特定买家的总消费金额 */
double trade_sum_amount_by_buyer(const Trade *head, const char *buyer_id)
{
    double total = 0.0;
    if (is_blank(buyer_id)) return 0.0;
    while (head != NULL) {
        if (head->status == TRADE_STATUS_COMPLETED && strcmp(head->buyer_id, buyer_id) == 0) {
            total += head->price;
        }
        head = head->next;
    }
    return total;
}

/** @brief 统计特定卖家的总收入金额 */
double trade_sum_amount_by_seller(const Trade *head, const char *seller_id)
{
    double total = 0.0;
    if (is_blank(seller_id)) return 0.0;
    while (head != NULL) {
        if (head->status == TRADE_STATUS_COMPLETED && strcmp(head->seller_id, seller_id) == 0) {
            total += head->price;
        }
        head = head->next;
    }
    return total;
}

/* ================= 打印与导出 ================= */

void trade_print_table_header(FILE *out)
{
    if (out == NULL) return;
    fprintf(out, "%-8s %-8s %-12s %-12s %-10s %-20s %-10s\n",
            "交易号", "图书号", "买家学号", "卖家学号", "金额", "交易时间", "状态");
    fprintf(out, "--------------------------------------------------------------------------------\n");
}

void trade_print_table_row(FILE *out, const Trade *trade)
{
    if (out == NULL || trade == NULL) return;
    fprintf(out, "%-8d %-8d %-12s %-12s %-10.2f %-20s %-10s\n",
            trade->trade_id, trade->book_id, trade->buyer_id, trade->seller_id,
            trade->price, trade->trade_time, trade_status_to_string(trade->status));
}

/** @brief 内部 visitor：用于 trade_print_table 的逐行打印 */
static void print_row_visitor(const Trade *trade, void *userdata)
{
    FILE *out = (FILE *)userdata;
    trade_print_table_row(out, trade);
}

/** @brief 打印过滤后的交易明细表 */
void trade_print_table(FILE *out, const Trade *head, const TradeQuery *query)
{
    if (out == NULL) return;
    trade_print_table_header(out);
    trade_query_each(head, query, print_row_visitor, out);
}

/** @brief 导出统计简报到文本文件 */
int trade_export_stats(const char *filename, const TradeStats *stats)
{
    FILE *fp;
    if (is_blank(filename) || stats == NULL) return 0;
    fp = fopen(filename, "w");
    if (fp == NULL) return 0;

    fprintf(fp, "校园二手书交易记录统计报表\n");
    fprintf(fp, "交易记录总数：%d\n", stats->total_count);
    fprintf(fp, "已完成交易数：%d\n", stats->completed_count);
    fprintf(fp, "已取消交易数：%d\n", stats->cancelled_count);
    fprintf(fp, "已完成交易金额：%.2f\n", stats->completed_amount);

    fclose(fp);
    return 1;
}

/* ================= CSV 文件持久化 ================= */

/**
 * @brief CSV 字段读取逻辑（处理引号转义）。
 */
static int csv_read_field(const char *line, int *pos, char *out, int out_size)
{
    int i = 0, quoted = 0;
    if (line == NULL || pos == NULL || out == NULL || out_size <= 0) return 0;

    out[0] = '\0';
    if (line[*pos] == '"') { quoted = 1; ++(*pos); }

    while (line[*pos] != '\0') {
        char ch = line[*pos];
        if (quoted) {
            if (ch == '"') {
                if (line[*pos + 1] == '"') { // 处理双引号转义
                    if (i < out_size - 1) out[i++] = '"';
                    *pos += 2; continue;
                }
                ++(*pos);
                if (line[*pos] == ',') ++(*pos);
                break;
            }
        } else if (ch == ',') { ++(*pos); break; }
        else if (ch == '\r' || ch == '\n') break;

        if (i < out_size - 1) out[i++] = ch;
        ++(*pos);
    }
    out[i] = '\0';
    return 1;
}

/** @brief CSV 字段写入逻辑（强制加引号保证安全） */
static void csv_write_field(FILE *fp, const char *text)
{
    const char *p = text == NULL ? "" : text;
    fputc('"', fp);
    while (*p != '\0') {
        if (*p == '"') { fputc('"', fp); fputc('"', fp); }
        else fputc(*p, fp);
        ++p;
    }
    fputc('"', fp);
}

/** @brief 识别 CSV 头部行 */
static int line_is_header(const char *line)
{
    return line != NULL && strstr(line, "trade_id,book_id,buyer_id") != NULL;
}

/** @brief 从 CSV 加载所有交易记录 */
int trade_load_from_csv(const char *filename, Trade **head)
{
    FILE *fp;
    char line[TRADE_LINE_BUFFER];
    int loaded = 0;

    if (is_blank(filename) || head == NULL) return -1;
    fp = fopen(filename, "r");
    if (fp == NULL) { *head = NULL; return 0; }

    trade_free_all(*head);
    *head = NULL;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char fields[TRADE_FIELD_COUNT][TRADE_TIME_LEN];
        int pos = 0, i;
        Trade *trade;

        if (line[0] == '\0' || line[0] == '\n' || line_is_header(line)) continue;

        for (i = 0; i < TRADE_FIELD_COUNT; ++i) {
            csv_read_field(line, &pos, fields[i], TRADE_TIME_LEN);
        }

        trade = trade_create(atoi(fields[0]), atoi(fields[1]), fields[2], fields[3], 
                             atof(fields[4]), fields[5], trade_status_from_string(fields[6]));
        
        if (trade == NULL) continue;
        if (!trade_insert(head, trade)) { free(trade); continue; }
        ++loaded;
    }
    fclose(fp);
    return loaded;
}

/** @brief 保存交易记录链表到 CSV 文件 */
int trade_save_to_csv(const char *filename, const Trade *head)
{
    FILE *fp;
    if (is_blank(filename)) return 0;
    fp = fopen(filename, "w");
    if (fp == NULL) return 0;

    fprintf(fp, "trade_id,book_id,buyer_id,seller_id,price,trade_time,status\n");
    while (head != NULL) {
        fprintf(fp, "%d,%d,", head->trade_id, head->book_id);
        csv_write_field(fp, head->buyer_id); fputc(',', fp);
        csv_write_field(fp, head->seller_id);
        fprintf(fp, ",%.2f,", head->price);
        csv_write_field(fp, head->trade_time); fputc(',', fp);
        csv_write_field(fp, trade_status_to_string(head->status)); fputc('\n', fp);
        head = head->next;
    }
    fclose(fp);
    return 1;
}