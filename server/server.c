#include "server.h"
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include "../networking/connection.h"
#include <stdlib.h>

typedef struct client {
    gint shell_stdin;
    gint shell_stdout;
    gint shell_stderr;
    Connection* connection;
    volatile bool canceled;
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
    client->connection = connection;
    client->canceled = false;
    return client;
}

gboolean isAthorized(GInputStream* istream)
{
    
}

void writing_loop_fd(Client* client, gint fd)
{
    GIOChannel* shell_out_channel = get_g_io_channel(fd);
    gchar* message = NULL;
    gsize length;
    GError* error = NULL;
    GIOStatus status;
    status = g_io_channel_read_line (shell_out_channel,
                                     &message,
                                     &length,
                                     NULL,
                                     &error);
    if (status == G_IO_STATUS_ERROR) {
        g_error(error->message);
        return;
    }
    while (status != G_IO_STATUS_EOF) {
        connection_send_message(client->connection, message, length);
        g_print("sent:%.*s", length, message);
        g_free(message);
        if (status == G_IO_STATUS_ERROR) {
            g_error(error->message);
            return;
        }
        status = g_io_channel_read_line (shell_out_channel,
                                         &message,
                                         &length,
                                         NULL,
                                         &error);
    }
    g_print("read end!\n");
    g_io_channel_unref(shell_out_channel);
}

gpointer client_reading_loop(Client* client)
{
    gssize charsRead;
    char* message;
    GIOChannel* shell_in_channel = get_g_io_channel(client->shell_stdin);
    gsize bytes_written;
    GError* error = NULL;
    while(message = 
          connection_read_message(client->connection, &charsRead)) {
        g_print("recieved:%.*s", charsRead, message);
        g_io_channel_write_chars(shell_in_channel,
                                 message,
                                 charsRead,
                                 &bytes_written,
                                 &error);
        g_io_channel_flush(shell_in_channel, &error);
        
        free(message);
    }
    g_io_channel_unref(shell_in_channel);
    return NULL;
}

gpointer client_writing_shell_out_loop(Client* client)
{
    writing_loop_fd(client, client->shell_stdout);
    return NULL;
}

gpointer client_writing_shell_err_loop(Client* client)
{
    writing_loop_fd(client, client->shell_stderr);
    return NULL;
}

//gpointer subproc_reading_loop(GSubprocess* subproc)
//{
//    g_test_subprocess();
//}

gpointer connectionHandler(gpointer connection)
{
    gint shell_stdin;
    gint shell_stdout;
    gint shell_stderr;
    gchar* argv[] = {"bash", NULL};
    GError* error = NULL;
    gboolean success = g_spawn_async_with_pipes(".", argv, NULL, G_SPAWN_SEARCH_PATH,
                                                NULL, NULL, NULL, &shell_stdin, &shell_stdout,
                                                &shell_stderr, &error);
    if (!success) {
        g_error(error->message);
        return NULL;
    }
    Client* client = client_new(shell_stdin, shell_stdout, shell_stderr, connection);
    GThread* readingThread = g_thread_new(NULL, (GThreadFunc) client_reading_loop, client);
    GThread* writingOutThread = g_thread_new(NULL, (GThreadFunc) client_writing_shell_out_loop, client);
    GThread* writingErrThread = g_thread_new(NULL, (GThreadFunc) client_writing_shell_err_loop, client);
    g_thread_join(readingThread);
    g_thread_join(writingOutThread);
    g_thread_join(writingErrThread);
    g_thread_unref(readingThread);
    g_thread_unref(writingOutThread);
    g_thread_unref(writingErrThread);
    connection_close(connection);
    free(connection);
    free(client);
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
