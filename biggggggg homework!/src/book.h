#ifndef BOOK_H
#define BOOK_H

#include <stdio.h>

/**
 * @name 全局常量定义
 * @{
 * 字符串字段长度统一放在头文件中，方便全项目复用和调整。
 * 长度包含结尾的空字符 '\0'。
 */
#define BOOK_TITLE_LEN       128  /**< 书名最大长度 */
#define BOOK_ISBN_LEN         32  /**< ISBN 长度 */
#define BOOK_AUTHOR_LEN       64  /**< 作者姓名长度 */
#define BOOK_PUBLISHER_LEN    64  /**< 出版社名称长度 */
#define BOOK_CATEGORY_LEN     64  /**< 分类名称长度（如：考研、计算机、文学） */
#define BOOK_STUDENT_ID_LEN   32  /**< 学号长度 */
#define BOOK_NAME_LEN         64  /**< 用户姓名长度 */
#define BOOK_TIME_LEN         32  /**< 时间字符串长度 (YYYY-MM-DD HH:MM:SS) */
#define BOOK_CONTACT_LEN      64  /**< 联系方式长度 */
#define BOOK_CONDITION_LEN    32  /**< 物品品相描述长度（如：九成新、有笔记） */
/** @} */

/**
 * @brief 图书交易状态枚举
 */
typedef enum BookStatus {
    BOOK_STATUS_ON_SALE = 0, /**< 在售：处于挂牌状态，可被购买 */
    BOOK_STATUS_SOLD = 1,    /**< 已售：交易已完成 */
    BOOK_STATUS_REMOVED = 2  /**< 已下架：卖家撤回，不再出售 */
} BookStatus;

/**
 * @brief 单本图书交易信息节点
 * @struct Book
 * 同时作为动态单向链表的节点使用。
 */
typedef struct Book {
    int id;                                 /**< 图书内部编号，全系统唯一自增 */
    char title[BOOK_TITLE_LEN];             /**< 书名 */
    char isbn[BOOK_ISBN_LEN];               /**< ISBN 编号 */
    char author[BOOK_AUTHOR_LEN];           /**< 作者 */
    char publisher[BOOK_PUBLISHER_LEN];     /**< 出版社 */
    char category[BOOK_CATEGORY_LEN];       /**< 分类 */
    char condition[BOOK_CONDITION_LEN];     /**< 新旧程度描述 */
    double price;                           /**< 售价（元） */
    char seller_id[BOOK_STUDENT_ID_LEN];    /**< 卖家学号 */
    char seller_name[BOOK_NAME_LEN];        /**< 卖家姓名 */
    char seller_contact[BOOK_CONTACT_LEN];  /**< 卖家联系方式 */
    char buyer_id[BOOK_STUDENT_ID_LEN];     /**< 买家学号（未成交时为空字符串） */
    BookStatus status;                      /**< 当前交易状态, 0-在售, 1-已售, 2-已下架 */
    char publish_time[BOOK_TIME_LEN];       /**< 信息发布时间 */
    char trade_time[BOOK_TIME_LEN];         /**< 成交或下架的操作时间 */
    struct Book *next;                      /**< 指向链表下一个节点的指针 */
} Book;

/**
 * @brief 查询过滤条件结构体
 * 字符串为空表示不限制该项；价格为负数表示不设置区间边界。
 */
typedef struct BookQuery {
    char keyword[BOOK_TITLE_LEN];      /**< 书名关键字（模糊匹配） */
    char isbn[BOOK_ISBN_LEN];          /**< ISBN 匹配 */
    char author[BOOK_AUTHOR_LEN];      /**< 作者关键字 */
    char publisher[BOOK_PUBLISHER_LEN];/**< 出版社关键字 */
    char category[BOOK_CATEGORY_LEN];  /**< 分类关键字 */
    char seller_id[BOOK_STUDENT_ID_LEN];/**< 指定卖家的学号 */
    BookStatus status;                 /**< 目标状态 */
    int use_status;                    /**< 是否启用状态过滤（1-启用，0-忽略） */
    double min_price;                  /**< 价格下限（负数不限） */
    double max_price;                  /**< 价格上限（负数不限） */
} BookQuery;

/**
 * @brief 统计结果结构体
 * 用于生成报表或展示全站概况。
 */
typedef struct BookStats {
    int total_count;           /**< 符合条件的图书总数 */
    int on_sale_count;         /**< 在售状态的数量 */
    int sold_count;            /**< 已售出的数量 */
    int removed_count;         /**< 已下架的数量 */
    double total_sold_amount;  /**< 累计成交的总金额 */
} BookStats;

/**
 * @brief 迭代器回调函数类型定义
 * @param book 当前遍历到的图书节点指针
 * @param userdata 用户自定义传递的上下文数据（如文件指针、计数器等）
 */
typedef void (*BookVisitFunc)(const Book *book, void *userdata);

/* ================= 1. 基础工具函数 ================= */

/** @brief 初始化查询参数，设置默认值 */
void book_init_query(BookQuery *query);

/** @brief 枚举状态转中文显示 */
const char *book_status_to_string(BookStatus status);

/** @brief 从用户输入或文件读取解析状态 */
BookStatus book_status_from_string(const char *text);

/** @brief 获取系统格式化时间字符串 */
void book_get_current_time(char *buffer, int size);

/* ================= 2. 链表生命周期管理 ================= */

/** @brief 创建新图书节点（分配内存） */
Book *book_create(int id, const char *title, const char *isbn, const char *author,
                  const char *publisher, const char *category, const char *condition,
                  double price, const char *seller_id, const char *seller_name,
                  const char *seller_contact, const char *publish_time);

/** @brief 销毁整个链表，释放内存 */
void book_free_all(Book *head);

/** @brief 获取链表节点总数 */
int book_count(const Book *head);

/** @brief 计算下一个可用的唯一 ID */
int book_next_id(const Book *head);

/* ================= 3. 核心增删改查接口 ================= */

/** @brief 将节点插入链表末尾 */
int book_insert(Book **head, Book *book);

/** @brief 按 ID 查找图书（返回可修改指针） */
Book *book_find_by_id(Book *head, int id);

/** @brief 按 ID 查找图书（返回只读指针） */
const Book *book_find_by_id_const(const Book *head, int id);

/** @brief 按 ID 物理删除节点（从内存移除） */
int book_delete_by_id(Book **head, int id);

/** @brief 用户发布图书接口（封装了生成 ID 和插入逻辑） */
int book_publish(Book **head, const char *title, const char *isbn, const char *author,
                 const char *publisher, const char *category, const char *condition,
                 double price, const char *seller_id, const char *seller_name,
                 const char *seller_contact);

/** @brief 购买图书业务逻辑 */
int book_buy(Book *head, int book_id, const char *buyer_id);

/** @brief 下架图书业务逻辑 */
int book_take_down(Book *head, int book_id, const char *seller_id);

/** @brief 修改图书价格 */
int book_update_price(Book *head, int book_id, const char *seller_id, double new_price);

/* ================= 4. 查询、排序、统计与输出 ================= */

/** @brief 核心过滤算法：检查单本图书是否符合 Query 条件 */
int book_matches_query(const Book *book, const BookQuery *query);

/** @brief 通用查询接口：对匹配项执行回调函数 */
int book_query_each(const Book *head, const BookQuery *query, BookVisitFunc visitor, void *userdata);

/** @brief 按价格排序链表 */
void book_sort_by_price(Book **head, int ascending);

/** @brief 按发布时间排序链表 */
void book_sort_by_publish_time(Book **head, int ascending);

/** @brief 计算一段时间内的交易统计指标 */
BookStats book_calculate_stats(const Book *head, const char *begin_time, const char *end_time);

/** @brief 打印表格表头到指定流（控制台或文件） */
void book_print_table_header(FILE *out);

/** @brief 打印单行图书数据到指定流 */
void book_print_table_row(FILE *out, const Book *book);

/** @brief 打印符合条件的完整图书列表 */
void book_print_table(FILE *out, const Book *head, const BookQuery *query);

/** @brief 将统计报表导出为 TXT 文件 */
int book_export_stats(const char *filename, const BookStats *stats);

/* ================= 5. CSV 数据持久化 ================= */

/** @brief 从硬盘 CSV 文件加载数据到链表（会清空当前链表） */
int book_load_from_csv(const char *filename, Book **head);

/** @brief 将当前链表数据序列化保存到 CSV 文件 */
int book_save_to_csv(const char *filename, const Book *head);

#endif /* BOOK_H */