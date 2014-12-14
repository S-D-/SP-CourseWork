#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include "networking/connection.h"
#include <string.h>
#include <stdlib.h>

GIOChannel* get_g_io_channel(gint fd)
{
#ifdef __WIN32__
    return g_io_channel_win32_new_fd(fd);
#elif __UNIX__
    return g_io_channel_unix_new(fd);
#endif
}

void writing_loop_fd(Connection* connection)
{
    GIOChannel* in_channel = get_g_io_channel(STDIN_FILENO);
    g_io_channel_set_encoding(in_channel, "", NULL);
    gchar* message = NULL;
    gsize length;
    GError* error = NULL;
    GIOStatus status;
    
    status = g_io_channel_read_line (in_channel,
                                     &message,
                                     &length,
                                     NULL,
                                     &error);
    while (status != G_IO_STATUS_EOF && status != G_IO_STATUS_ERROR) {
        connection_send_message(connection, message, length);
        g_free(message);
        status = g_io_channel_read_line (in_channel,
                                         &message,
                                         &length,
                                         NULL,
                                         &error);
        if (status == G_IO_STATUS_EOF) {
            connection_send_message(connection, "exit\n", 5);
        }
    }
    printf("stdin finished\n");
    connection_close(connection);
    printf("connection closed\n");
    g_io_channel_unref(in_channel);
}

gpointer client_reading_loop(Connection* connection)
{
    size_t charsRead;
    char* message;
    GIOChannel* stdout_channel = get_g_io_channel(STDOUT_FILENO);
    g_io_channel_set_encoding(stdout_channel, "", NULL);
    gsize bytes_written;
    GError* error = NULL;
    while(message = 
          connection_read_message(connection, &charsRead)) {
//        if (!memcmp(message, "SP_SERVFinished", 15)) {
//            free(message);
//            break;
//        }
        g_io_channel_write_chars(stdout_channel,
                                 message,
                                 charsRead,
                                 &bytes_written,
                                 &error);
        g_io_channel_flush(stdout_channel, &error);
        free(message);
    }
    g_print("read end!\n");
    g_io_channel_unref(stdout_channel);
    return NULL;
}

gpointer client_writing_shell_out_loop(Connection* connection)
{
    writing_loop_fd(connection);
    return NULL;
}

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
        g_print (error->message);
        g_error_free (error);
    } else {
        g_print ("Connection successful!\n");
    }
  
    Connection connection = {
        gSockConnection
    };
    
    GThread* readingThread = g_thread_new(NULL, (GThreadFunc) client_reading_loop, &connection);
    GThread* writingOutThread = g_thread_new(NULL, (GThreadFunc) client_writing_shell_out_loop, &connection);
    g_thread_join(readingThread);
    g_thread_join(writingOutThread);
    g_thread_unref(readingThread);
    g_thread_unref(writingOutThread);
    //connection_close(&connection);
    g_object_unref(client);
    return 0;
}
