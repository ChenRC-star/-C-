#ifndef USER_H
#define USER_H

#include <stdio.h>

/* 用户字段长度。统一定义便于 user.c、HTTP 路由和页面表单共同使用。 */
#define USER_STUDENT_ID_LEN  32
#define USER_NAME_LEN        64
#define USER_PASSWORD_LEN    128
#define USER_PHONE_LEN       32
#define USER_EMAIL_LEN       64
#define USER_TIME_LEN        32
#define USER_TOKEN_LEN       64

/* 用户角色。普通学生用于买卖图书，管理员可用于后台管理扩展。 */
typedef enum UserRole {
    USER_ROLE_STUDENT = 0,
    USER_ROLE_ADMIN = 1
} UserRole;

/* 用户链表节点。 */
typedef struct User {
    char student_id[USER_STUDENT_ID_LEN];       /* 学号，作为唯一账号 */
    char name[USER_NAME_LEN];                   /* 姓名或昵称 */
    char password_hash[USER_PASSWORD_LEN];      /* 密码哈希值，不直接保存明文密码 */
    char phone[USER_PHONE_LEN];                 /* 手机或 QQ 等联系方式 */
    char email[USER_EMAIL_LEN];                 /* 邮箱，可为空 */
    UserRole role;                              /* 用户角色 */
    char register_time[USER_TIME_LEN];          /* 注册时间 */
    char last_login_time[USER_TIME_LEN];        /* 最近登录时间 */
    struct User *next;                          /* 下一用户节点 */
} User;

/* 登录会话节点。HTTP 项目可用 token 判断用户是否已登录。 */
typedef struct UserSession {
    char token[USER_TOKEN_LEN];                 /* 登录令牌 */
    char student_id[USER_STUDENT_ID_LEN];       /* 当前会话所属学号 */
    char create_time[USER_TIME_LEN];            /* 创建时间 */
    struct UserSession *next;                   /* 下一会话节点 */
} UserSession;

/* 遍历用户时使用的回调函数。 */
typedef void (*UserVisitFunc)(const User *user, void *userdata);

/* 基础工具函数 */
const char *user_role_to_string(UserRole role);
UserRole user_role_from_string(const char *text);
void user_get_current_time(char *buffer, int size);
void user_make_password_hash(const char *password, char *buffer, int size);
int user_check_password(const User *user, const char *password);

/* 用户链表生命周期 */
User *user_create(const char *student_id,
                  const char *name,
                  const char *password,
                  const char *phone,
                  const char *email,
                  UserRole role);
void user_free_all(User *head);
int user_count(const User *head);

/* 注册、登录和用户资料 */
int user_insert(User **head, User *user);
User *user_find_by_student_id(User *head, const char *student_id);
const User *user_find_by_student_id_const(const User *head, const char *student_id);
User *user_find_by_name(User *head, const char *name);
int user_exists(const User *head, const char *student_id);
int user_register(User **head,
                  const char *student_id,
                  const char *name,
                  const char *password,
                  const char *phone,
                  const char *email);
User *user_login(User *head, const char *student_id, const char *password);
int user_change_password(User *head,
                         const char *student_id,
                         const char *old_password,
                         const char *new_password);
int user_update_profile(User *head,
                        const char *student_id,
                        const char *name,
                        const char *phone,
                        const char *email);
int user_delete_by_student_id(User **head, const char *student_id);
void user_each(const User *head, UserVisitFunc visitor, void *userdata);
void user_print_table(FILE *out, const User *head);

/* 登录会话。简单项目可用，也可以只在前端 localStorage 保存 student_id。 */
UserSession *session_create(const char *student_id);
void session_free_all(UserSession *head);
int session_insert(UserSession **head, UserSession *session);
const UserSession *session_find_by_token(const UserSession *head, const char *token);
int session_remove_by_token(UserSession **head, const char *token);
const User *session_get_user(const UserSession *sessions, const User *users, const char *token);

/* CSV 文件持久化 */
int user_load_from_csv(const char *filename, User **head);
int user_save_to_csv(const char *filename, const User *head);

#endif
