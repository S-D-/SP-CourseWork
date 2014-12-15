#include "server.h"
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include "../networking/connection.h"
#include <stdlib.h>
#include <string.h>

typedef struct client {
    gint shell_stdin;
    gint shell_stdout;
    gint shell_stderr;
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
    g_io_channel_set_encoding(shell_out_channel, "", NULL);
    gchar* message = NULL;
    gsize length;
    GError* error = NULL;
    GIOStatus status;
    status = g_io_channel_read_line (shell_out_channel,
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
        status = g_io_channel_read_line (shell_out_channel,
                                         &message,
                                         &length,
                                         NULL,
                                         &error);
        cmp = length < 7 ? -1 : memcmp(message, "SP_SERV", 7); 
    }
//    connection_send_message(client->connection, "SP_SERVFinished", 15);
    g_print("read end!\n");
    g_io_channel_unref(shell_out_channel);
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
    GIOChannel* shell_in_channel = get_g_io_channel(client->shell_stdin);
    g_io_channel_set_encoding(shell_in_channel, "", NULL);
    gsize bytes_written;
    GError* error = NULL;
    while(message = 
          connection_read_message(client->connection, &charsRead)) {
        g_print("recieved:%.*s", charsRead, message);
#ifdef __UNIX__
        if(client->isShellActive) {
            write_to_shell_in_if_active(shell_in_channel,
                                        message,
                                        charsRead,
                                        client);
        } else {
#endif
        g_io_channel_write_chars(shell_in_channel,
                                 message,
                                 charsRead,
                                 &bytes_written,
                                 &error);
#ifdef __UNIX__
        }
#endif
        g_io_channel_flush(shell_in_channel, &error);
        
        free(message);
    }
    printf("reading from client finished\n");
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
