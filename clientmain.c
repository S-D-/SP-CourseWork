#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include "networking/connection.h"
#include <string.h>

int main(int argc, char *argv[])
{ 
    g_type_init();
    GError * error = NULL;
  
    /* create a new connection */
    GSocketConnection * gSockConnection = NULL;
    GSocketClient * client = g_socket_client_new();
  
    /* connect to the host */
    gSockConnection = g_socket_client_connect_to_host (client,
                                                 (gchar*)"localhost",
                                                  3336, /* your port goes here */
                                                  NULL,
                                                  &error);
  
    /* don't forget to check for errors */
    if (error != NULL) {
        g_error (error->message);
        g_error_free (error);
    } else {
        g_print ("Connection successful!\n");
    }
  
    Connection connection = {
        gSockConnection
    };
    char buf[100];
    int msgSize;
    do {
        fgets (buf, 100, stdin);
        msgSize = strlen(buf);
        connection_send_message(&connection, buf, strlen(buf));
    } while(msgSize != 1 && buf[0] != '\n');
    connection_close(&connection);
    g_object_unref(client);
    return 0;
}
