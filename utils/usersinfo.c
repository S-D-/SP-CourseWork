#include "usersinfo.h"
#include "usersxmlparser.h"
#include <stdlib.h>

UsersInfo* USERS_INFO;

UsersInfo* users_info_new()
{
    return g_hash_table_new(g_str_hash, g_str_equal);
}

gboolean free_entry(gpointer key, gpointer value, gpointer user_data)
{
    UserInfo* user = (UserInfo*)value;
    free(user->name);
    free(user->password);
    free(value);
    return TRUE;
}

void users_info_free(UsersInfo* users)
{
    g_hash_table_foreach_remove(users, free_entry, NULL);
    g_hash_table_unref(users);
}
