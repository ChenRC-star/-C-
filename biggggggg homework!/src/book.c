#include "book.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BOOK_LINE_BUFFER 2048
#define BOOK_FIELD_COUNT 15

/* ================= 内部工具函数 (Static) ================= */

/**
 * @brief 安全复制字符串，保证目标字符串一定以 '\0' 结尾。
 * @param dest 目标缓冲区
 * @param dest_size 缓冲区总大小
 * @param src 源字符串
 */
static void copy_text(char *dest, int dest_size, const char *src)
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

/**
 * @brief 判断字符串是否为空或仅包含空字符。
 * @return 1 为空，0 不为空
 */
static int is_blank(const char *text)
{
    return text == NULL || text[0] == '\0';
}

/**
 * @brief 字符级大小写不敏感比较（仅限 ASCII 英文）。
 */
static int char_equal_ignore_case(char a, char b)
{
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

/**
 * @brief 字符串子串查找（忽略大小写）。
 * @param source 源文本
 * @param keyword 搜索关键字
 * @return 1 包含，0 不包含
 */
static int contains_ignore_case(const char *source, const char *keyword)
{
    size_t i, j, source_len, keyword_len;

    if (is_blank(keyword)) return 1;
    if (is_blank(source)) return 0;

    source_len = strlen(source);
    keyword_len = strlen(keyword);
    if (keyword_len > source_len) return 0;

    for (i = 0; i <= source_len - keyword_len; ++i) {
        for (j = 0; j < keyword_len; ++j) {
            if (!char_equal_ignore_case(source[i + j], keyword[j])) break;
        }
        if (j == keyword_len) return 1;
    }
    return 0;
}

/**
 * @brief 判断指定时间是否在 [begin_time, end_time] 闭区间内。
 * @note 由于时间格式统一为 YYYY-MM-DD，可直接使用 strcmp 进行字典序比较。
 */
static int time_in_range(const char *time_text, const char *begin_time, const char *end_time)
{
    // 为空或格式不合法的时间戳视为不在范围内
    if (is_blank(time_text)) return 0;
    // 仅当 begin_time 不为空且 time_text 小于 begin_time 时返回 0
    if (!is_blank(begin_time) && strcmp(time_text, begin_time) < 0) return 0;
    // 仅当 end_time 不为空且 time_text 大于 end_time 时返回 0
    if (!is_blank(end_time) && strcmp(time_text, end_time) > 0) return 0;
    return 1;
}

/* ================= 基础逻辑函数 ================= */

/**
 * @brief 初始化查询结构体。将状态设为“在售”，价格区间设为无效值(-1)。
 */
void book_init_query(BookQuery *query)
{
    // 由于 BookQuery 中包含多个字符串字段，直接 memset 为 0 可以保证它们都以 '\0' 结尾。
    if (query == NULL) return;
    memset(query, 0, sizeof(BookQuery));
    // 默认状态为在售，且不启用状态过滤
    query->status = BOOK_STATUS_ON_SALE;
    // 默认价格区间为无效值，表示不限制价格范围
    query->use_status = 0;
    // 价格下限和上限设为负数，表示不限制价格范围
    query->min_price = -1.0;
    query->max_price = -1.0;
}

/**
 * @brief 将图书状态枚举值转换为可读的中文字符串。
 */
const char *book_status_to_string(BookStatus status)
{
    switch (status) {
    case BOOK_STATUS_ON_SALE:  return "在售";
    case BOOK_STATUS_SOLD:     return "已售";
    case BOOK_STATUS_REMOVED:  return "已下架";
    default:                   return "未知";
    }
}

/**
 * @brief 从字符串解析图书状态（支持数字或中文关键字）。
 */
BookStatus book_status_from_string(const char *text)
{
    // 允许输入 "0"/"1"/"2" 或 "在售"/"已售"/"已下架"（不区分大小写）
    // 任何不匹配的输入都默认为 BOOK_STATUS_ON_SALE
    if (text == NULL) return BOOK_STATUS_ON_SALE;
    if (strcmp(text, "1") == 0 || strcmp(text, "已售") == 0 || strcmp(text, "sold") == 0) {
        return BOOK_STATUS_SOLD;
    }
    if (strcmp(text, "2") == 0 || strcmp(text, "已下架") == 0 || strcmp(text, "removed") == 0) {
        return BOOK_STATUS_REMOVED;
    }
    return BOOK_STATUS_ON_SALE;
}

/**
 * @brief 获取系统当前日期和时间，格式为 "YYYY-MM-DD HH:MM:SS"。
 */
void book_get_current_time(char *buffer, int size)
{
    // 获取当前系统时间并格式化输出到 buffer 中
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

/**
 * @brief 创建一个新的 Book 节点并分配内存。
 * @return 成功返回节点指针，失败（或参数不合法）返回 NULL。
 */
Book *book_create(int id, const char *title, const char *isbn, const char *author,
                  const char *publisher, const char *category, const char *condition,
                  double price, const char *seller_id, const char *seller_name,
                  const char *seller_contact, const char *publish_time)
{
    Book *book;
    if (is_blank(title) || is_blank(seller_id) || price < 0.0) return NULL;

    book = (Book *)malloc(sizeof(Book));
    if (book == NULL) return NULL;

    memset(book, 0, sizeof(Book));
    book->id = id;
    copy_text(book->title, BOOK_TITLE_LEN, title);
    copy_text(book->isbn, BOOK_ISBN_LEN, isbn);
    copy_text(book->author, BOOK_AUTHOR_LEN, author);
    copy_text(book->publisher, BOOK_PUBLISHER_LEN, publisher);
    copy_text(book->category, BOOK_CATEGORY_LEN, category);
    copy_text(book->condition, BOOK_CONDITION_LEN, condition);
    book->price = price;
    copy_text(book->seller_id, BOOK_STUDENT_ID_LEN, seller_id);
    copy_text(book->seller_name, BOOK_NAME_LEN, seller_name);
    copy_text(book->seller_contact, BOOK_CONTACT_LEN, seller_contact);
    book->status = BOOK_STATUS_ON_SALE;

    if (is_blank(publish_time)) {
        book_get_current_time(book->publish_time, BOOK_TIME_LEN);
    } else {
        copy_text(book->publish_time, BOOK_TIME_LEN, publish_time);
    }
    return book;
}

/**
 * @brief 释放整条链表的动态内存。
 */
void book_free_all(Book *head)
{
    Book *current = head;
    while (current != NULL) {
        Book *next = current->next;
        free(current);
        current = next;
    }
}

/**
 * @brief 计算链表中节点的总数。
 */
int book_count(const Book *head)
{
    int count = 0;
    while (head != NULL) {
        ++count;
        head = head->next;
    }
    return count;
}

/**
 * @brief 遍历链表寻找当前最大的 ID 并加 1，用于生成新书编号。
 */
int book_next_id(const Book *head)
{
    int max_id = 0;
    while (head != NULL) {
        if (head->id > max_id) max_id = head->id;
        head = head->next;
    }
    return max_id + 1;
}

/**
 * @brief 尾插法：将一个图书节点插入链表末尾。
 */
int book_insert(Book **head, Book *book)
{
    Book *tail;
    if (head == NULL || book == NULL) return 0;

    book->next = NULL;
    if (*head == NULL) {
        *head = book;
        return 1;
    }
    tail = *head;
    while (tail->next != NULL) tail = tail->next;
    tail->next = book;
    return 1;
}

/**
 * @brief 根据 ID 查找图书节点。
 */
Book *book_find_by_id(Book *head, int id)
{
    while (head != NULL) {
        if (head->id == id) return head;
        head = head->next;
    }
    return NULL;
}

/**
 * @brief 根据 ID 查找图书节点（只读版本）。
 */
const Book *book_find_by_id_const(const Book *head, int id)
{
    while (head != NULL) {
        if (head->id == id) return head;
        head = head->next;
    }
    return NULL;
}

/**
 * @brief 根据 ID 从链表中删除一个节点并释放内存。
 */
int book_delete_by_id(Book **head, int id)
{
    Book *current, *previous = NULL;
    if (head == NULL) return 0;

    current = *head;
    while (current != NULL) {
        if (current->id == id) {
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

/* ================= 业务功能逻辑 ================= */

/**
 * @brief 封装“发布图书”业务：生成 ID、创建节点并插入链表。
 */
int book_publish(Book **head, const char *title, const char *isbn, const char *author,
                 const char *publisher, const char *category, const char *condition,
                 double price, const char *seller_id, const char *seller_name, const char *seller_contact)
{
    Book *book;
    int id;
    if (head == NULL || is_blank(title) || is_blank(seller_id) || price < 0.0) return 0;

    id = book_next_id(*head);
    book = book_create(id, title, isbn, author, publisher, category, condition,
                       price, seller_id, seller_name, seller_contact, NULL);
    if (book == NULL) return 0;

    return book_insert(head, book);
}

/**
 * @brief 执行“购买”动作：更新买家 ID、状态和交易时间。
 * @note 限制：不能买自己发布的书，且书必须是在售状态。
 */
int book_buy(Book *head, int book_id, const char *buyer_id)
{
    Book *book = book_find_by_id(head, book_id);
    if (book == NULL || is_blank(buyer_id)) return 0;
    if (book->status != BOOK_STATUS_ON_SALE) return 0;
    // 卖家不能购买自己的书
    if (strcmp(book->seller_id, buyer_id) == 0) return 0;

    copy_text(book->buyer_id, BOOK_STUDENT_ID_LEN, buyer_id);
    book->status = BOOK_STATUS_SOLD;
    book_get_current_time(book->trade_time, BOOK_TIME_LEN);
    return 1;
}

/**
 * @brief 执行“下架”动作：只有卖家本人可下架在售图书。
 */
int book_take_down(Book *head, int book_id, const char *seller_id)
{
    Book *book = book_find_by_id(head, book_id);
    if (book == NULL || is_blank(seller_id)) return 0;
    if (book->status != BOOK_STATUS_ON_SALE) return 0;
    // 只有卖家本人可下架自己的书
    if (strcmp(book->seller_id, seller_id) != 0) return 0;

    book->status = BOOK_STATUS_REMOVED;
    book_get_current_time(book->trade_time, BOOK_TIME_LEN);
    return 1;
}

/**
 * @brief 更新图书价格：仅限卖家本人操作在售图书。
 */
int book_update_price(Book *head, int book_id, const char *seller_id, double new_price)
{
    Book *book = book_find_by_id(head, book_id);
    if (book == NULL || is_blank(seller_id) || new_price < 0.0) return 0;
    if (book->status != BOOK_STATUS_ON_SALE) return 0;
    if (strcmp(book->seller_id, seller_id) != 0) return 0;

    book->price = new_price;
    return 1;
}

/**
 * @brief 判断某本书是否满足查询结构体（Query）中的所有过滤条件。
 * @return 1 匹配，0 不匹配
 */

 // 该函数实现了基于 BookQuery 结构体的多条件过滤逻辑，支持字符串模糊匹配、状态过滤和价格区间过滤。
 // 通过将过滤逻辑封装在一个函数中，可以在 book_query_each 等函数中复用，保持代码清晰和易维护。
int book_matches_query(const Book *book, const BookQuery *query)
{
    if (book == NULL) return 0;
    if (query == NULL) return 1;

    if (!contains_ignore_case(book->title, query->keyword)) return 0;
    if (!contains_ignore_case(book->isbn, query->isbn)) return 0;
    if (!contains_ignore_case(book->author, query->author)) return 0;
    if (!contains_ignore_case(book->publisher, query->publisher)) return 0;
    if (!contains_ignore_case(book->category, query->category)) return 0;
    if (!is_blank(query->seller_id) && strcmp(book->seller_id, query->seller_id) != 0) return 0;
    if (query->use_status && book->status != query->status) return 0;
    if (query->min_price >= 0.0 && book->price < query->min_price) return 0;
    if (query->max_price >= 0.0 && book->price > query->max_price) return 0;

    return 1;
}

/**
 * @brief 遍历查询函数：对满足条件的节点调用 visitor 回调函数。
 */
int book_query_each(const Book *head, const BookQuery *query, BookVisitFunc visitor, void *userdata)
{
    // 该函数实现了基于 BookQuery 结构体的链表遍历和条件过滤逻辑。
    // 对于每个节点，先调用 book_matches_query 判断是否满足查询条件，如果满足则调用 visitor 回调函数进行处理。
    int count = 0;
    while (head != NULL) {
        if (book_matches_query(head, query)) {
            ++count;
            if (visitor != NULL) visitor(head, userdata);
        }
        head = head->next;
    }
    return count;
}

/**
 * @brief 辅助排序：交换两个节点的内部数据（不交换指针指向）。
 */
// 该函数通过交换两个 Book 节点的内容来实现排序算法中的节点交换操作，避免了调整链表指针的复杂性。
// 作用 ： 在 book_sort_by_price 和 book_sort_by_publish_time 中被调用，用于根据价格或发布时间对链表进行冒泡排序。
static void swap_book_data(Book *a, Book *b)
{
    Book temp;
    Book *next_a, *next_b;
    if (a == NULL || b == NULL || a == b) return;

    next_a = a->next; next_b = b->next;
    temp = *a; *a = *b; *b = temp;
    a->next = next_a; b->next = next_b;
}

/**
 * @brief 按价格对链表进行冒泡排序。
 * @param ascending 1 为升序，0 为降序
 */
void book_sort_by_price(Book **head, int ascending)
{
    Book *i, *j;
    if (head == NULL || *head == NULL) return;

    for (i = *head; i != NULL; i = i->next) {
        for (j = i->next; j != NULL; j = j->next) {
            int should_swap = ascending ? (i->price > j->price) : (i->price < j->price);
            if (should_swap) swap_book_data(i, j);
        }
    }
}

/**
 * @brief 按发布时间对链表进行冒泡排序。
 */
void book_sort_by_publish_time(Book **head, int ascending)
{
    Book *i, *j;
    if (head == NULL || *head == NULL) return;

    for (i = *head; i != NULL; i = i->next) {
        for (j = i->next; j != NULL; j = j->next) {
            int cmp = strcmp(i->publish_time, j->publish_time);
            int should_swap = ascending ? (cmp > 0) : (cmp < 0);
            if (should_swap) swap_book_data(i, j);
        }
    }
}

/**
 * @brief 统计指定时间范围内的交易数据（上架数、成交额等）。
 */
// 该函数通过遍历链表并检查每个节点的时间戳，来计算指定时间范围内的交易统计数据。
BookStats book_calculate_stats(const Book *head, const char *begin_time, const char *end_time)
{
    BookStats stats;
    memset(&stats, 0, sizeof(BookStats));

    while (head != NULL) {
        int include_publish = time_in_range(head->publish_time, begin_time, end_time);
        int include_trade = time_in_range(head->trade_time, begin_time, end_time);

        if (include_publish) {
            ++stats.total_count;
            if (head->status == BOOK_STATUS_ON_SALE) ++stats.on_sale_count;
            else if (head->status == BOOK_STATUS_REMOVED) ++stats.removed_count;
        }
        if (head->status == BOOK_STATUS_SOLD && include_trade) {
            ++stats.sold_count;
            stats.total_sold_amount += head->price;
        }
        head = head->next;
    }
    return stats;
}

/* ================= 界面输出与文件 I/O ================= */

/**
 * @brief 打印图书列表的表格头。
 */
void book_print_table_header(FILE *out)
{
    if (out == NULL) return;
    fprintf(out, "%-6s %-24s %-16s %-12s %-10s %-8s %-10s %-12s %-12s\n",
            "编号", "书名", "ISBN", "作者", "分类", "价格", "状态", "卖家学号", "发布时间");
    fprintf(out, "----------------------------------------------------------------------------------------------------------------\n");
}

/**
 * @brief 打印单条图书记录。
 */
void book_print_table_row(FILE *out, const Book *book)
{
    if (out == NULL || book == NULL) return;
    fprintf(out, "%-6d %-24s %-16s %-12s %-10s %-8.2f %-10s %-12s %-12s\n",
            book->id, book->title, book->isbn, book->author, book->category,
            book->price, book_status_to_string(book->status), book->seller_id, book->publish_time);
}

/**
 * @brief 内部 visitor 回调，用于 book_print_table。
 */
static void print_row_visitor(const Book *book, void *userdata)
{
    FILE *out = (FILE *)userdata;
    book_print_table_row(out, book);
}

/**
 * @brief 打印符合条件的图书表格到指定输出流。
 */
void book_print_table(FILE *out, const Book *head, const BookQuery *query)
{
    if (out == NULL) return;
    book_print_table_header(out);
    book_query_each(head, query, print_row_visitor, out);
}

/**
 * @brief 将统计信息导出为文本文档报表。
 */
int book_export_stats(const char *filename, const BookStats *stats)
{
    FILE *fp;
    if (is_blank(filename) || stats == NULL) return 0;

    fp = fopen(filename, "w");
    if (fp == NULL) return 0;

    fprintf(fp, "校园二手书交易统计报表\n");
    fprintf(fp, "总上架数量：%d\n", stats->total_count);
    fprintf(fp, "在售数量：%d\n", stats->on_sale_count);
    fprintf(fp, "已售数量：%d\n", stats->sold_count);
    fprintf(fp, "已下架数量：%d\n", stats->removed_count);
    fprintf(fp, "总交易金额：%.2f\n", stats->total_sold_amount);

    fclose(fp);
    return 1;
}

/**
 * @brief 解析 CSV 格式的一个字段，处理可能的引号转义。
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
                if (line[*pos + 1] == '"') { // 处理转义双引号 ""
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

/**
 * @brief 将字段写出到 CSV，对内容加引号并处理内部引号转义。
 */
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

/**
 * @brief 判断当前行是否为 CSV 的表头。
 */
static int line_is_header(const char *line)
{
    return line != NULL && strstr(line, "id,title,isbn") != NULL;
}

/**
 * @brief 从 CSV 文件加载所有图书数据到链表。
 * @return 成功加载的记录数量，失败返回 -1 或 0。
 */
int book_load_from_csv(const char *filename, Book **head)
{
    FILE *fp;
    char line[BOOK_LINE_BUFFER];
    int loaded = 0;

    if (is_blank(filename) || head == NULL) return -1;

    fp = fopen(filename, "r");
    if (fp == NULL) { *head = NULL; return 0; }

    book_free_all(*head);
    *head = NULL;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char fields[BOOK_FIELD_COUNT][BOOK_TITLE_LEN];
        int pos = 0, i;
        Book *book;

        if (line[0] == '\0' || line[0] == '\n' || line_is_header(line)) continue;

        for (i = 0; i < BOOK_FIELD_COUNT; ++i) {
            csv_read_field(line, &pos, fields[i], BOOK_TITLE_LEN);
        }

        book = book_create(atoi(fields[0]), fields[1], fields[2], fields[3],
                           fields[4], fields[5], fields[6], atof(fields[7]),
                           fields[8], fields[9], fields[10], fields[13]);
        if (book == NULL) continue;

        copy_text(book->buyer_id, BOOK_STUDENT_ID_LEN, fields[11]);
        book->status = book_status_from_string(fields[12]);
        copy_text(book->trade_time, BOOK_TIME_LEN, fields[14]);
        book_insert(head, book);
        ++loaded;
    }
    fclose(fp);
    return loaded;
}

/**
 * @brief 将链表中的所有图书数据保存到 CSV 文件。
 * @return 1 成功，0 失败。
 */
int book_save_to_csv(const char *filename, const Book *head)
{
    FILE *fp;
    if (is_blank(filename)) return 0;

    fp = fopen(filename, "w");
    if (fp == NULL) return 0;

    // 写入表头
    fprintf(fp, "id,title,isbn,author,publisher,category,condition,price,seller_id,seller_name,seller_contact,buyer_id,status,publish_time,trade_time\n");

    while (head != NULL) {
        fprintf(fp, "%d,", head->id);
        csv_write_field(fp, head->title); fputc(',', fp);
        csv_write_field(fp, head->isbn); fputc(',', fp);
        csv_write_field(fp, head->author); fputc(',', fp);
        csv_write_field(fp, head->publisher); fputc(',', fp);
        csv_write_field(fp, head->category); fputc(',', fp);
        csv_write_field(fp, head->condition);
        fprintf(fp, ",%.2f,", head->price);
        csv_write_field(fp, head->seller_id); fputc(',', fp);
        csv_write_field(fp, head->seller_name); fputc(',', fp);
        csv_write_field(fp, head->seller_contact); fputc(',', fp);
        csv_write_field(fp, head->buyer_id); fputc(',', fp);
        csv_write_field(fp, book_status_to_string(head->status)); fputc(',', fp);
        csv_write_field(fp, head->publish_time); fputc(',', fp);
        csv_write_field(fp, head->trade_time); fputc('\n', fp);

        head = head->next;
    }
    fclose(fp);
    return 1;
}