#include "user.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define USER_LINE_BUFFER 1024
#define USER_FIELD_COUNT 8

/* ================= 内部工具函数 (Static) ================= */

/**
 * @brief 安全复制字符串，避免缓冲区溢出，并保证目标字符串以 '\0' 结尾。
 * @param dest 目标缓冲区
 * @param dest_size 缓冲区大小
 * @param src 源字符串
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
 * @brief 判断字符串是否为空或为 NULL。
 * @return 1 为空，0 不为空
 */
static int is_blank(const char *text)
{
    return text == NULL || text[0] == '\0';
}

/**
 * @brief 校验学号合法性：仅限数字和字母，长度需在 3 到预设最大值之间。
 */
static int is_valid_student_id(const char *student_id)
{
    int i, len;
    if (is_blank(student_id)) return 0;

    len = (int)strlen(student_id);
    if (len < 3 || len >= USER_STUDENT_ID_LEN) return 0;

    for (i = 0; i < len; ++i) {
        if (!isalnum((unsigned char)student_id[i])) return 0;
    }
    return 1;
}

/**
 * @brief 校验密码强度：当前规则为长度不少于 6 位。
 */
static int is_valid_password(const char *password)
{
    return password != NULL && strlen(password) >= 6;
}

/* ================= 核心工具函数 ================= */

/**
 * @brief 获取系统当前日期时间，格式为 "YYYY-MM-DD HH:MM:SS"。
 */
void user_get_current_time(char *buffer, int size)
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

/**
 * @brief 将用户角色枚举值转换为中文描述。
 */
const char *user_role_to_string(UserRole role)
{
    switch (role) {
    case USER_ROLE_ADMIN:   return "管理员";
    case USER_ROLE_STUDENT: return "学生";
    default:                return "学生";
    }
}

/**
 * @brief 从字符串解析用户角色（支持数字、中文及英文关键字）。
 */
UserRole user_role_from_string(const char *text)
{
    if (text == NULL) return USER_ROLE_STUDENT;
    if (strcmp(text, "1") == 0 || strcmp(text, "管理员") == 0 || strcmp(text, "admin") == 0) {
        return USER_ROLE_ADMIN;
    }
    return USER_ROLE_STUDENT;
}

/**
 * @brief 简单 FNV-1a 哈希算法：用于对密码进行不可逆加密存储。
 * @param password 明文密码
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 */
void user_make_password_hash(const char *password, char *buffer, int size)
{
    // 这里使用了一个非常简单的哈希算法（FNV-1a），仅用于演示目的。实际项目中应使用更安全的哈希算法
    // （如 bcrypt、scrypt 或 Argon2）。
    // FNV-1a 32-bit hash
    // 2166136261 是 FNV-1a 的初始值，16777619 是 FNV-1a 的素数乘数。
    // 输出格式为 8 位十六进制字符串。
    // 注意：此处的哈希算法非常简单，不能抵抗暴力破解或彩虹表攻击。仅适用于教学示例。
    // 此处哈希的详细实现步骤如下
    // 1. 初始化哈希值为 FNV-1a 的初始值。
    // 2. 遍历密码字符串的每个字符：
    // 3. 将当前哈希值与字符的 ASCII 值进行异或运算。
    // 4. 将结果乘以 FNV-1a 的素数乘数。
    // 5. 最后将计算得到的哈希值格式化为十六进制字符串输出。
    // 这种哈希算法的优点是简单且速度快，但缺点是安全性较低，容易被破解。
    // 实际项目中应使用更强大的哈希算法来保护用户密码。
    unsigned long hash = 2166136261u;
    const unsigned char *p = (const unsigned char *)(password == NULL ? "" : password);
    if (buffer == NULL || size <= 0) return;

    while (*p != '\0') {
        hash ^= (unsigned long)(*p);
        hash *= 16777619u;
        ++p;
    }
    snprintf(buffer, (size_t)size, "%08lx", hash);
}

/**
 * @brief 验证用户输入的明文密码与存储的哈希值是否匹配。
 */
int user_check_password(const User *user, const char *password)
{
    char hash[USER_PASSWORD_LEN];
    if (user == NULL || password == NULL) return 0;
    user_make_password_hash(password, hash, sizeof(hash));
    return strcmp(user->password_hash, hash) == 0;
}

/* ================= 用户链表操作 ================= */

/**
 * @brief 创建一个新的用户节点（分配内存并初始化字段）。
 * @param student_id 学号，必须唯一且合法
 * @param name 姓名或昵称，不能为空
 * @param password 明文密码，必须符合强度要求
 * @param phone 联系方式，可以为空
 * @param email 邮箱地址，可以为空
 * @param role 用户角色，决定权限等级
 * @return 成功返回用户节点指针，失败返回 NULL
 */
User *user_create(const char *student_id, const char *name, const char *password,
                  const char *phone, const char *email, UserRole role)
{
    User *user;
    if (!is_valid_student_id(student_id) || is_blank(name) || !is_valid_password(password)) {
        return NULL;
    }

    user = (User *)malloc(sizeof(User));
    if (user == NULL) return NULL;

    memset(user, 0, sizeof(User));
    copy_text(user->student_id, USER_STUDENT_ID_LEN, student_id);
    copy_text(user->name, USER_NAME_LEN, name);
    user_make_password_hash(password, user->password_hash, USER_PASSWORD_LEN);
    copy_text(user->phone, USER_PHONE_LEN, phone);
    copy_text(user->email, USER_EMAIL_LEN, email);
    user->role = role;
    user_get_current_time(user->register_time, USER_TIME_LEN);
    user->next = NULL;
    return user;
}

/**
 * @brief 释放用户链表的所有内存。
 */
void user_free_all(User *head)
{
    User *current = head;
    while (current != NULL) {
        User *next = current->next;
        free(current);
        current = next;
    }
}

/**
 * @brief 统计用户总数。
 */
int user_count(const User *head)
{
    int count = 0;
    while (head != NULL) {
        ++count;
        head = head->next;
    }
    return count;
}

/**
 * @brief 将用户插入链表尾部（会自动检查学号是否已存在）。
 */
int user_insert(User **head, User *user)
{
    User *tail;
    if (head == NULL || user == NULL) return 0;
    if (user_exists(*head, user->student_id)) return 0;

    user->next = NULL;
    if (*head == NULL) {
        *head = user;
        return 1;
    }
    tail = *head;
    while (tail->next != NULL) tail = tail->next;
    tail->next = user;
    return 1;
}

/**
 * @brief 根据学号查找用户。
 */
User *user_find_by_student_id(User *head, const char *student_id)
{
    if (is_blank(student_id)) return NULL;
    while (head != NULL) {
        if (strcmp(head->student_id, student_id) == 0) return head;
        head = head->next;
    }
    return NULL;
}

/**
 * @brief 根据学号查找用户（只读版）。
 */
const User *user_find_by_student_id_const(const User *head, const char *student_id)
{
    if (is_blank(student_id)) return NULL;
    while (head != NULL) {
        if (strcmp(head->student_id, student_id) == 0) return head;
        head = head->next;
    }
    return NULL;
}

/**
 * @brief 根据姓名精确查找用户。
 */
User *user_find_by_name(User *head, const char *name)
{
    if (is_blank(name)) return NULL;
    while (head != NULL) {
        if (strcmp(head->name, name) == 0) return head;
        head = head->next;
    }
    return NULL;
}

/**
 * @brief 检查指定学号的用户是否存在。
 */
int user_exists(const User *head, const char *student_id)
{
    return user_find_by_student_id_const(head, student_id) != NULL;
}

/* ================= 业务逻辑 ================= */

/**
 * @brief 用户注册：校验学号唯一性并添加新学生账户。
 */
int user_register(User **head, const char *student_id, const char *name,
                  const char *password, const char *phone, const char *email)
{
    User *user;
    if (head == NULL || user_exists(*head, student_id)) return 0;

    user = user_create(student_id, name, password, phone, email, USER_ROLE_STUDENT);
    if (user == NULL) return 0;

    if (!user_insert(head, user)) {
        free(user);
        return 0;
    }
    return 1;
}

/**
 * @brief 用户登录：验证账号密码，并更新“最近登录时间”。
 * @return 登录成功返回用户指针，失败返回 NULL。
 */
User *user_login(User *head, const char *student_id, const char *password)
{
    User *user = user_find_by_student_id(head, student_id);
    if (user == NULL || !user_check_password(user, password)) return NULL;

    user_get_current_time(user->last_login_time, USER_TIME_LEN);
    return user;
}

/**
 * @brief 修改密码：需校验旧密码并符合新密码规则。
 */
int user_change_password(User *head, const char *student_id, const char *old_password, const char *new_password)
{
    User *user = user_find_by_student_id(head, student_id);
    if (user == NULL || !user_check_password(user, old_password) || !is_valid_password(new_password)) {
        return 0;
    }
    user_make_password_hash(new_password, user->password_hash, USER_PASSWORD_LEN);
    return 1;
}

/**
 * @brief 更新用户基本资料（姓名、电话、邮箱）。
 */
int user_update_profile(User *head, const char *student_id, const char *name, const char *phone, const char *email)
{
    User *user = user_find_by_student_id(head, student_id);
    if (user == NULL) return 0;

    if (!is_blank(name)) copy_text(user->name, USER_NAME_LEN, name);
    if (phone != NULL) copy_text(user->phone, USER_PHONE_LEN, phone);
    if (email != NULL) copy_text(user->email, USER_EMAIL_LEN, email);
    return 1;
}

/**
 * @brief 根据学号删除用户。
 */
int user_delete_by_student_id(User **head, const char *student_id)
{
    User *current, *previous = NULL;
    if (head == NULL || is_blank(student_id)) return 0;

    current = *head;
    while (current != NULL) {
        if (strcmp(current->student_id, student_id) == 0) {
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
 * @brief 用户列表迭代器：对每个用户执行指定的回调函数。
 */
void user_each(const User *head, UserVisitFunc visitor, void *userdata)
{
    if (visitor == NULL) return;
    while (head != NULL) {
        visitor(head, userdata);
        head = head->next;
    }
}

/**
 * @brief 打印用户列表表格到输出流（如控制台）。
 */
void user_print_table(FILE *out, const User *head)
{
    if (out == NULL) return;
    fprintf(out, "%-14s %-12s %-16s %-24s %-10s %-20s %-20s\n",
            "学号", "姓名", "电话", "邮箱", "角色", "注册时间", "最近登录");
    fprintf(out, "----------------------------------------------------------------------------------------------------------------\n");
    while (head != NULL) {
        fprintf(out, "%-14s %-12s %-16s %-24s %-10s %-20s %-20s\n",
                head->student_id, head->name, head->phone, head->email,
                user_role_to_string(head->role), head->register_time, head->last_login_time);
        head = head->next;
    }
}

/* ================= Session 会话管理 ================= */

/**
 * @brief 生成一个随机 Session Token。
 * 组合：学号 + 当前时间 + 随机数，再进行哈希。
 */
static void make_session_token(const char *student_id, char *buffer, int size)
{
    char seed[USER_TOKEN_LEN * 2];
    char now[USER_TIME_LEN];
    user_get_current_time(now, sizeof(now));
    srand((unsigned int)time(NULL) ^ (unsigned int)rand());
    snprintf(seed, sizeof(seed), "%s-%s-%d", student_id == NULL ? "" : student_id, now, rand());
    user_make_password_hash(seed, buffer, size);
}

/**
 * @brief 创建 Session：用于记录用户登录后的凭证。
 */
UserSession *session_create(const char *student_id)
{
    UserSession *session;
    if (is_blank(student_id)) return NULL;

    session = (UserSession *)malloc(sizeof(UserSession));
    if (session == NULL) return NULL;

    memset(session, 0, sizeof(UserSession));
    copy_text(session->student_id, USER_STUDENT_ID_LEN, student_id);
    make_session_token(student_id, session->token, USER_TOKEN_LEN);
    user_get_current_time(session->create_time, USER_TIME_LEN);
    session->next = NULL;
    return session;
}

/**
 * @brief 释放所有 Session 内存。
 */
void session_free_all(UserSession *head)
{
    UserSession *current = head;
    while (current != NULL) {
        UserSession *next = current->next;
        free(current);
        current = next;
    }
}

/**
 * @brief 将 Session 节点插入链表。
 */
int session_insert(UserSession **head, UserSession *session)
{
    UserSession *tail;
    if (head == NULL || session == NULL) return 0;
    session->next = NULL;
    if (*head == NULL) {
        *head = session;
        return 1;
    }
    tail = *head;
    while (tail->next != NULL) tail = tail->next;
    tail->next = session;
    return 1;
}

/**
 * @brief 根据 Token 寻找 Session。
 */
const UserSession *session_find_by_token(const UserSession *head, const char *token)
{
    if (is_blank(token)) return NULL;
    while (head != NULL) {
        if (strcmp(head->token, token) == 0) return head;
        head = head->next;
    }
    return NULL;
}

/**
 * @brief 根据 Token 移除 Session（注销登录时使用）。
 */
int session_remove_by_token(UserSession **head, const char *token)
{
    UserSession *current, *previous = NULL;
    if (head == NULL || is_blank(token)) return 0;

    current = *head;
    while (current != NULL) {
        if (strcmp(current->token, token) == 0) {
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
 * @brief 鉴权：通过 Token 获取对应的用户信息指针。
 */
const User *session_get_user(const UserSession *sessions, const User *users, const char *token)
{
    const UserSession *session = session_find_by_token(sessions, token);
    if (session == NULL) return NULL;
    return user_find_by_student_id_const(users, session->student_id);
}

/* ================= CSV 持久化 ================= */

/**
 * @brief 读取 CSV 字段，支持双引号转义及包含逗号的内容。
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
                if (line[*pos + 1] == '"') { // 转义双引号 ""
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
 * @brief 写出 CSV 字段，统一加双引号。
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

/** @brief 检查行是否为 CSV 表头 */
static int line_is_header(const char *line)
{
    return line != NULL && strstr(line, "student_id,name,password_hash") != NULL;
}

/**
 * @brief 从 CSV 文件读取用户列表并重建链表。
 */
int user_load_from_csv(const char *filename, User **head)
{
    FILE *fp;
    char line[USER_LINE_BUFFER];
    int loaded = 0;

    if (is_blank(filename) || head == NULL) return -1;
    fp = fopen(filename, "r");
    if (fp == NULL) { *head = NULL; return 0; }

    user_free_all(*head);
    *head = NULL;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char fields[USER_FIELD_COUNT][USER_PASSWORD_LEN];
        int pos = 0, i;
        User *user;

        if (line[0] == '\0' || line[0] == '\n' || line_is_header(line)) continue;

        for (i = 0; i < USER_FIELD_COUNT; ++i) {
            csv_read_field(line, &pos, fields[i], USER_PASSWORD_LEN);
        }

        user = (User *)malloc(sizeof(User));
        if (user == NULL) continue;

        memset(user, 0, sizeof(User));
        copy_text(user->student_id, USER_STUDENT_ID_LEN, fields[0]);
        copy_text(user->name, USER_NAME_LEN, fields[1]);
        copy_text(user->password_hash, USER_PASSWORD_LEN, fields[2]);
        copy_text(user->phone, USER_PHONE_LEN, fields[3]);
        copy_text(user->email, USER_EMAIL_LEN, fields[4]);
        user->role = user_role_from_string(fields[5]);
        copy_text(user->register_time, USER_TIME_LEN, fields[6]);
        copy_text(user->last_login_time, USER_TIME_LEN, fields[7]);
        user->next = NULL;

        if (!is_valid_student_id(user->student_id) || is_blank(user->name) || is_blank(user->password_hash)) {
            free(user); continue;
        }
        if (!user_insert(head, user)) { free(user); continue; }
        ++loaded;
    }
    fclose(fp);
    return loaded;
}

/**
 * @brief 将用户链表保存到 CSV 文件。
 */
int user_save_to_csv(const char *filename, const User *head)
{
    FILE *fp;
    if (is_blank(filename)) return 0;
    fp = fopen(filename, "w");
    if (fp == NULL) return 0;

    fprintf(fp, "student_id,name,password_hash,phone,email,role,register_time,last_login_time\n");
    while (head != NULL) {
        csv_write_field(fp, head->student_id); fputc(',', fp);
        csv_write_field(fp, head->name); fputc(',', fp);
        csv_write_field(fp, head->password_hash); fputc(',', fp);
        csv_write_field(fp, head->phone); fputc(',', fp);
        csv_write_field(fp, head->email); fputc(',', fp);
        csv_write_field(fp, user_role_to_string(head->role)); fputc(',', fp);
        csv_write_field(fp, head->register_time); fputc(',', fp);
        csv_write_field(fp, head->last_login_time); fputc('\n', fp);
        head = head->next;
    }
    fclose(fp);
    return 1;
}