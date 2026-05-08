#include "storage.h"

#include "utils.h"

#include <string.h>

void storage_init(AppData *data)
{
    if (data == NULL) {
        return;
    }

    memset(data, 0, sizeof(AppData));
    utils_safe_copy(data->books_file, sizeof(data->books_file), STORAGE_DEFAULT_BOOKS_FILE);
    utils_safe_copy(data->users_file, sizeof(data->users_file), STORAGE_DEFAULT_USERS_FILE);
    utils_safe_copy(data->trades_file, sizeof(data->trades_file), STORAGE_DEFAULT_TRADES_FILE);
}

void storage_set_files(AppData *data, const char *books_file, const char *users_file, const char *trades_file)
{
    if (data == NULL) {
        return;
    }

    if (!utils_is_blank(books_file)) {
        utils_safe_copy(data->books_file, sizeof(data->books_file), books_file);
    }
    if (!utils_is_blank(users_file)) {
        utils_safe_copy(data->users_file, sizeof(data->users_file), users_file);
    }
    if (!utils_is_blank(trades_file)) {
        utils_safe_copy(data->trades_file, sizeof(data->trades_file), trades_file);
    }
}

void storage_free(AppData *data)
{
    if (data == NULL) {
        return;
    }

    book_free_all(data->books);
    user_free_all(data->users);
    trade_free_all(data->trades);
    session_free_all(data->sessions);
    data->books = NULL;
    data->users = NULL;
    data->trades = NULL;
    data->sessions = NULL;
}

int storage_load_all(AppData *data)
{
    int books_loaded;
    int users_loaded;
    int trades_loaded;

    if (data == NULL) {
        return 0;
    }

    books_loaded = book_load_from_csv(data->books_file, &data->books);
    users_loaded = user_load_from_csv(data->users_file, &data->users);
    trades_loaded = trade_load_from_csv(data->trades_file, &data->trades);

    return books_loaded >= 0 && users_loaded >= 0 && trades_loaded >= 0;
}

int storage_save_books(const AppData *data)
{
    return data != NULL && book_save_to_csv(data->books_file, data->books);
}

int storage_save_users(const AppData *data)
{
    return data != NULL && user_save_to_csv(data->users_file, data->users);
}

int storage_save_trades(const AppData *data)
{
    return data != NULL && trade_save_to_csv(data->trades_file, data->trades);
}

int storage_save_all(const AppData *data)
{
    if (data == NULL) {
        return 0;
    }

    return storage_save_books(data) && storage_save_users(data) && storage_save_trades(data);
}

int storage_buy_book(AppData *data, int book_id, const char *buyer_id)
{
    Book *book;

    if (data == NULL) {
        return 0;
    }

    if (!book_buy(data->books, book_id, buyer_id)) {
        return 0;
    }

    book = book_find_by_id(data->books, book_id);
    if (book == NULL || !trade_add_from_book(&data->trades, book)) {
        return 0;
    }

    return storage_save_books(data) && storage_save_trades(data);
}

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
                         const char *seller_contact)
{
    if (data == NULL) {
        return 0;
    }

    if (!book_publish(&data->books, title, isbn, author, publisher, category,
                      condition, price, seller_id, seller_name, seller_contact)) {
        return 0;
    }

    return storage_save_books(data);
}

int storage_take_down_book(AppData *data, int book_id, const char *seller_id)
{
    if (data == NULL) {
        return 0;
    }

    if (!book_take_down(data->books, book_id, seller_id)) {
        return 0;
    }

    return storage_save_books(data);
}

int storage_register_user(AppData *data,
                          const char *student_id,
                          const char *name,
                          const char *password,
                          const char *phone,
                          const char *email)
{
    if (data == NULL) {
        return 0;
    }

    if (!user_register(&data->users, student_id, name, password, phone, email)) {
        return 0;
    }

    return storage_save_users(data);
}
