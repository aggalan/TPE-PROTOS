#ifndef ADMIN_H
#define ADMIN_H

#define USER_FILE "./src/authentication/users.txt"

int admin_add_user(const char *username, const char *password);
int admin_del_user(const char *username);
char *admin_list_users(void);

#endif // ADMIN_H
