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

gpointer connection_reading_loop(Client* client)
{
    size_t charsRead;
    while(char* message = 
          connection_read_message(client->connection, &charsRead)) {
        fwrite(message, sizeof (char), 
                     charsRead, client->shell_stdin);
        free(message);
    }
    fclose(client->shell_stdin);
    return NULL;
}

gpointer connection_writing_loop(Client* client)
{
    const buff_size = 50;
    char buffer[buf_size];
    do {
        fgets (buffer, buf_size, client->shell_stdout);
        int msg_len = strlen(buffer);
        connection_send_message(client->connection, buffer, msg_len);
        size_t charsRead;
    } while (!feof(client->shell_stdout));
    fclose(client->shell_stdout);
    fclose(client->shell_stderr);
    return NULL;
}

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
    GThread* readingThread = g_thread_new(NULL, connection_reading_loop, client);
    GThread* writingThread = g_thread_new(NULL, connection_writing_loop, client);
    g_thread_join(readingThread);
    g_thread_join(writingThread);
    g_thread_unref(readingThread);
    g_thread_unref(writingThread);
    connection_close(connection);
    free(connection);
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
