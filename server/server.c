#include "server.h"
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include "../networking/connection.h"
#include <stdlib.h>
#include <string.h>
#include "../utils/usersinfo.h"

typedef struct client {
    gint shell_stdin;
    gint shell_stdout;
    gint shell_stderr;
    GIOChannel* shell_in_channel;
    GIOChannel* shell_out_channel;
    GIOChannel* shell_err_channel;
    Connection* connection;
    volatile bool canceled;
    volatile bool isShellActive;
} Client;

GIOChannel* get_g_io_channel(gint fd)
{
#ifdef __WIN32__
    return g_io_channel_win32_new_fd(fd);
#elif __UNIX__
    return g_io_channel_unix_new(fd);
#endif
}

Client* client_new(gint shell_stdin,
                   gint shell_stdout,
                   gint shell_stderr,
                   Connection* connection)
{
    Client* client = malloc(sizeof *client);
    client->shell_stdin = shell_stdin;
    client->shell_stdout = shell_stdout;
    client->shell_stderr = shell_stderr;
    GIOChannel* shell_out_channel = get_g_io_channel(shell_stdout);
    g_io_channel_set_encoding(shell_out_channel, "", NULL);
    client->shell_out_channel = shell_out_channel;
    GIOChannel* shell_err_channel = get_g_io_channel(shell_stderr);
    g_io_channel_set_encoding(shell_err_channel, "", NULL);
    client->shell_err_channel = shell_err_channel;
    GIOChannel* shell_in_channel = get_g_io_channel(shell_stdin);
    g_io_channel_set_encoding(shell_in_channel, "", NULL);
    client->shell_in_channel = shell_in_channel;
    client->connection = connection;
    client->canceled = false;
    return client;
}

void client_free(Client* client)
{
    connection_close(client->connection);
    g_io_channel_unref(client->shell_out_channel);
    g_io_channel_unref(client->shell_err_channel);
    g_io_channel_unref(client->shell_in_channel);
    free(client->connection); //TODO mb g_free()
    free(client);
}

gboolean isAthorized(Connection* connection)
{    
    size_t login_size;
    char* login = connection_read_message(connection, &login_size);
    if (login == NULL) {
        connection_send_message(connection, "Authentification failed\n", 
                                sizeof("Authentification failed\n"));
        return FALSE;
    }
    size_t passwd_size;
    char* password = connection_read_message(connection, &passwd_size);
    if (password == NULL) {
        connection_send_message(connection, "Authentification failed\n", 
                                sizeof("Authentification failed\n"));
        return FALSE;
    }
    /* size without '\0' */
    login_size--;
    passwd_size--;
    
    printf("login:%s\n", login);
    printf("password:%s\n", password);
    
    UserInfo* user_info = g_hash_table_lookup(USERS_INFO, login);
    if (user_info == NULL) {
        connection_send_message(connection, "Authentification failed\n", 
                                sizeof("Authentification failed\n"));
        return FALSE;
    }
    if (user_info->passwd_size != passwd_size) {
        connection_send_message(connection, "Authentification failed\n", 
                                sizeof("Authentification failed\n"));
        return FALSE;
    }
    if (memcmp(user_info->password, password, passwd_size)) {
        connection_send_message(connection, "Authentification failed\n", 
                                sizeof("Authentification failed\n"));
        return FALSE;
    }
    char welcome_msg[11 + login_size];
    snprintf(welcome_msg, sizeof(welcome_msg), "Welcome %.*s\n", login_size, login);
    connection_send_message(connection, welcome_msg, sizeof(welcome_msg));
    return TRUE;
}

void writing_loop_io_channel(Client* client, GIOChannel* channel)
{
    gchar* message = NULL;
    gsize length;
    GError* error = NULL;
    GIOStatus status;
    status = g_io_channel_read_line (channel,
                                     &message,
                                     &length,
                                     NULL,
                                     &error);
    //TODO: unbuffered reading
    if (status == G_IO_STATUS_ERROR) {
        g_print(error->message);
        return;
    }
    int cmp = length < 7 ? -1 : memcmp(message, "SP_SERV", 7); 
    
    while (status != G_IO_STATUS_EOF) {
        if (!cmp) {
            client->isShellActive = TRUE;
            connection_send_message(client->connection, &message[7], length-8);
        } else {
            connection_send_message(client->connection, message, length);
        }
        g_free(message);
        if (status == G_IO_STATUS_ERROR) {
            g_error(error->message);
            return;
        }
        status = g_io_channel_read_line (channel,
                                         &message,
                                         &length,
                                         NULL,
                                         &error);
        cmp = length < 7 ? -1 : memcmp(message, "SP_SERV", 7); 
    }
//    connection_send_message(client->connection, "SP_SERVFinished", 15);
    g_print("read end!\n");
    //g_io_channel_unref(shell_out_channel);
}

void write_to_shell_in_if_active(GIOChannel* channel,
                                 char* message,
                                 size_t length,
                                 Client* client)
{
    int new_line_idx = -1;
    for (int i = 0; i < length; ++i) {
        if (message[i] == '\n') {
            new_line_idx = i;
            break;
        }
    }
    if (new_line_idx == -1) {
        g_print("ERROR!!!");
        return;
    }
    gsize bytes_written;
    GError* error = NULL;
    g_io_channel_write_chars(channel, //write without newline char
                             message,
                             new_line_idx,
                             &bytes_written,
                             &error);
    const char prompt_request[] = ";echo \"SP_SERV$(pwd)> \"\n";
    const int prompt_req_size = sizeof(prompt_request) - 1;
    g_io_channel_write_chars(channel,
                             prompt_request,
                             prompt_req_size,
                             &bytes_written,
                             &error);
    client->isShellActive = FALSE;
    if (new_line_idx != length - 1) {
        g_io_channel_write_chars(channel, //write remainder
                                 &message[new_line_idx+1],
                                 length - new_line_idx - 1,
                                 &bytes_written,
                                 &error);
    }
}

gpointer client_reading_loop(Client* client)
{
    size_t charsRead;
    char* message;
    gsize bytes_written;
    GError* error = NULL;
    while(message = 
          connection_read_message(client->connection, &charsRead)) {
        g_print("recieved:%.*s", charsRead, message);
#ifdef __UNIX__
        if(client->isShellActive) {
            write_to_shell_in_if_active(client->shell_in_channel,
                                        message,
                                        charsRead,
                                        client);
        } else {
#endif
        g_io_channel_write_chars(client->shell_in_channel,
                                 message,
                                 charsRead,
                                 &bytes_written,
                                 &error);
#ifdef __UNIX__
        }
#endif
        g_io_channel_flush(client->shell_in_channel, &error);
        
        free(message);
    }
    printf("reading from client finished\n");
    //g_io_channel_unref(shell_in_channel);
    return NULL;
}

gpointer client_writing_shell_out_loop(Client* client)
{
    writing_loop_io_channel(client, client->shell_out_channel);
    return NULL;
}

gpointer client_writing_shell_err_loop(Client* client)
{
    writing_loop_io_channel(client, client->shell_err_channel);
    return NULL;
}

//gpointer subproc_reading_loop(GSubprocess* subproc)
//{
//    g_test_subprocess();
//}

gpointer connectionHandler(gpointer connection)
{
    if (!isAthorized(connection)) {
        connection_close(connection);
        free(connection);
        return NULL;
    }
    gint shell_stdin;
    gint shell_stdout;
    gint shell_stderr;
#ifdef __WIN32__
    gchar* argv[] = {"cmd.exe", NULL};
#elif __UNIX__
    gchar* argv[] = {"zsh", NULL};
#endif
    GError* error = NULL;
    gboolean success = g_spawn_async_with_pipes(".", argv, NULL, G_SPAWN_SEARCH_PATH,
                                                NULL, NULL, NULL, &shell_stdin, &shell_stdout,
                                                &shell_stderr, &error);
    if (!success) {
        g_error(error->message);
        return NULL;
    }
    Client* client = client_new(shell_stdin, shell_stdout, shell_stderr, connection);
    client->isShellActive = TRUE;
    GThread* readingThread = g_thread_new(NULL, (GThreadFunc) client_reading_loop, client);
    GThread* writingOutThread = g_thread_new(NULL, (GThreadFunc) client_writing_shell_out_loop, client);
    GThread* writingErrThread = g_thread_new(NULL, (GThreadFunc) client_writing_shell_err_loop, client);
    g_print("threads started\n");
    g_thread_join(readingThread);
    g_thread_join(writingOutThread);
    g_thread_join(writingErrThread);
    g_thread_unref(readingThread);
    g_thread_unref(writingOutThread);
    g_thread_unref(writingErrThread);
    g_print("Client disconnected\n");
    client_free(client);
    return NULL;
}

gboolean incoming_callback  (GSocketService *service,
                             GSocketConnection *connection,
                             GObject *source_object,
                             gpointer user_data)
{
    g_print("Received Connection from client!\n");
    g_object_ref(connection);
    Connection* con = malloc(sizeof *con);
    con->gSockConnection = connection;
    GThread* handlerThread = g_thread_new(NULL, connectionHandler, con);
    g_thread_unref(handlerThread);
    return FALSE;
}

void start(Server* server)
{
    GError * error = NULL;
    GSocketService * service = g_socket_service_new();
    g_socket_listener_add_inet_port((GSocketListener*)service,
                                    server->port, /* your port goes here */
                                    NULL,
                                    &error);
    if (error != NULL)
    {
        g_error(error->message);
    }
    g_signal_connect(service,
                     "incoming",
                     G_CALLBACK(incoming_callback),
                     NULL);
    g_socket_service_start (service);
}

void cancel(Server *server)
{
    server->canceled = true;
}
