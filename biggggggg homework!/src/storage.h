#ifndef STORAGE_H
#define STORAGE_H

#include "book.h"
#include "trade.h"
#include "user.h"

/* 默认数据文件位置，HTTP 服务器从项目根目录启动时可直接使用。 */
#define STORAGE_DEFAULT_DATA_DIR     "data"
#define STORAGE_DEFAULT_BOOKS_FILE   "data/books.csv"
#define STORAGE_DEFAULT_USERS_FILE   "data/users.csv"
#define STORAGE_DEFAULT_TRADES_FILE  "data/trades.csv"

/* 后端运行时的全部内存数据。 */
typedef struct AppData {
    Book *books;                  /* 图书链表 */
    User *users;                  /* 用户链表 */
    Trade *trades;                /* 交易链表 */
    UserSession *sessions;        /* 登录会话链表 */
    char books_file[260];         /* 图书 CSV 路径 */
    char users_file[260];         /* 用户 CSV 路径 */
    char trades_file[260];        /* 交易 CSV 路径 */
} AppData;

void storage_init(AppData *data);
void storage_set_files(AppData *data, const char *books_file, const char *users_file, const char *trades_file);
void storage_free(AppData *data);

int storage_load_all(AppData *data);
int storage_save_all(const AppData *data);
int storage_save_books(const AppData *data);
int storage_save_users(const AppData *data);
int storage_save_trades(const AppData *data);

/* 组合业务：购买图书成功后同步写入交易记录并保存两个 CSV。 */
int storage_buy_book(AppData *data, int book_id, const char *buyer_id);
int storage_publish_book(AppData *data,
                         const char *title,
                         const char *isbn,
                         const char *author,
                         const char *publisher,
                         const char *category,
                         const char *condition,
                         double price,
                         const char *seller_id,
                         const char *seller_name,
                         const char *seller_contact);
int storage_take_down_book(AppData *data, int book_id, const char *seller_id);
int storage_register_user(AppData *data,
                          const char *student_id,
                          const char *name,
                          const char *password,
                          const char *phone,
                          const char *email);

#endif
