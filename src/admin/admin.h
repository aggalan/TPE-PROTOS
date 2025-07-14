#ifndef ADMIN_H
#define ADMIN_H

#include <stddef.h>

#define USER_FILE      "./src/authentication/users.txt"
#define ADMIN_FILE     "./src/admin/admins.txt"
#define USER_MAX_LEN   15
#define ADMIN_MAX      10
#define USER_MAX  10



int admin_add_user(const char *username, const char *password, const char *user_file);
int admin_del_user(const char *username, const char *user_file);
char *admin_list_users(const char *user_file);
int validate_user(const char *username, const char *password, const char *user_file);

#endif // ADMIN_H