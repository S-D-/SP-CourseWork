#ifndef USERSINFO_H
#define USERSINFO_H

#include <glib.h>

typedef struct user_info {
    char* name;
    size_t name_size;
    char* password;
    size_t passwd_size;
} UserInfo;

typedef GHashTable UsersInfo;

extern UsersInfo* USERS_INFO;

UsersInfo* users_info_new();
void users_info_free(UsersInfo* users);

#endif //USERSINFO_H
