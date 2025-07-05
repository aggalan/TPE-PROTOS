#include "../authentication/authentication_parser.h"

typedef enum {USER, ADMIN} UserRole;

typedef struct user
{
    UserRole role;  // Rol del usuario (USER o ADMIN)
    char username[USER_MAX_LEN + 1];  // Nombre de usuario
    char password[PASSWORD_MAX_LEN + 1];  // Contraseña
}User;

int user_add(const char *username, const char *password, UserRole role);
int user_remove(const char *username);
bool user_authenticate(const char *username, const char *password, User *out_user);
char *get_user_list(UserRole role); // si manda un NULL, devuelve todos los usuarios
UserRole get_user_role(const char *username);
int set_user_role(const char *username, UserRole role);
void free_user_manager(char *user_list);