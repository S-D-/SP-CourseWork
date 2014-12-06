#include "server.h"
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include "connection.h"
#include <stdlib.h>

gboolean isAthorized(GInputStream* istream)
{
    
}

gpointer connectionHandler(gpointer connection)
{
    size_t msgSize;
    char* msg;
    do {
        msg = connection_read_message(connection, &msgSize);
        printf("%.*s", msgSize, msg);
    } while(msgSize != 1 && msg[0] != '\n');
    printf("Bye!\n");
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
