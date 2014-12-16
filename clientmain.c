#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include "networking/connection.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    Connection* connection;
    GIOChannel* stdin_channel;
    GIOChannel* stdout_channel;
} AppIO;

GIOChannel* get_g_io_channel(gint fd)
{
#ifdef __WIN32__
    return g_io_channel_win32_new_fd(fd);
#elif __UNIX__
    return g_io_channel_unix_new(fd);
#endif
}

gpointer client_reading_loop(AppIO* app_io)
{
    size_t charsRead;
    char* message;
    gsize bytes_written;
    GError* error = NULL;
    while(message = 
          connection_read_message(app_io->connection, &charsRead)) {
//        if (!memcmp(message, "SP_SERVFinished", 15)) {
//            free(message);
//            break;
//        }
        g_io_channel_write_chars(app_io->stdout_channel,
                                 message,
                                 charsRead,
                                 &bytes_written,
                                 &error);
        g_io_channel_flush(app_io->stdout_channel, &error);
        free(message);
    }
    g_print("read end!\n");
    return NULL;
}

gpointer client_writing_shell_out_loop(AppIO* app_io)
{
    gchar* message = NULL;
    gsize length;
    GError* error = NULL;
    GIOStatus status;
    
    status = g_io_channel_read_line (app_io->stdin_channel,
                                     &message,
                                     &length,
                                     NULL,
                                     &error);
    while (status != G_IO_STATUS_EOF && status != G_IO_STATUS_ERROR) {
        connection_send_message(app_io->connection, message, length);
        g_free(message);
        status = g_io_channel_read_line (app_io->stdin_channel,
                                         &message,
                                         &length,
                                         NULL,
                                         &error);
        if (status == G_IO_STATUS_EOF) {
            connection_send_message(app_io->connection, "exit\n", 5);
        }
    }
    printf("stdin finished\n");
    connection_close(app_io->connection);
    printf("connection closed\n");
    return NULL;
}

typedef struct {
    char* host;
    char* port;
    char* login;
    const char* password;
} ClientParams;
const char* calc_md5(char* str, GChecksum* checksum)
{
    g_checksum_update(checksum, str, strlen(str));
    return g_checksum_get_string(checksum); 
}

int handle_app_params(int argc, char** argv, ClientParams* client_params, GChecksum* checksum)
{
    int params_handled = 0;
    for (int i = 0; i < argc - 1; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            client_params->host = argv[i+1];
            int j;
            for (j = 0; argv[i][j] != '\0'; ++j) {
                if (argv[i][j] == ':') {
                    argv[i][j] = '\0';
                    client_params->port = &argv[i][j+1];
                    break;
                }
            }
            if (argv[i][j] == '\0') {
                client_params->port = "3336"; //default port
            }
            params_handled += 2;
        } else if (strcmp(argv[i], "-u") == 0) {
            client_params->login = argv[i + 1];
            printf("login:%s\n", argv[i+1]);
            params_handled++;
        } else if (strcmp(argv[i], "-p") == 0) {
            client_params->password = calc_md5(argv[i + 1], checksum);
            params_handled++;
        }
    }
    if (params_handled != 4) {
        return -1; // error
    }
    return 0;
}

void authentificate(AppIO* app_io, ClientParams* client_params)
{
    connection_send_message(app_io->connection, client_params->login, 
                            strlen(client_params->login) + 1);
    connection_send_message(app_io->connection, client_params->password,
                            strlen(client_params->password) + 1);
}

int main(int argc, char *argv[])
{ 
    g_type_init();
    ClientParams client_params;
    GChecksum* checksum = g_checksum_new(G_CHECKSUM_MD5);
    if (handle_app_params(argc, argv, &client_params, checksum) == -1) {
        printf("Usage: SPClient -h <host:port> -u <login> -p <password>\n");
        return -1;
    }
    GError * error = NULL;
    
    /* create a new connection */
    GSocketConnection * gSockConnection = NULL;
    GSocketClient * client = g_socket_client_new();
  
    /* connect to the host */
    gSockConnection = g_socket_client_connect_to_host (client,
                                                       client_params.host,
                                                       atoi(client_params.port), /* your port goes here */
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
    
    GIOChannel* stdout_channel = get_g_io_channel(STDOUT_FILENO);
    g_io_channel_set_encoding(stdout_channel, "", NULL);
    GIOChannel* stdin_channel = get_g_io_channel(STDIN_FILENO);
    g_io_channel_set_encoding(stdin_channel, "", NULL);
    AppIO app_io = {&connection, stdin_channel, stdout_channel};
    
    authentificate(&app_io, &client_params);
    g_checksum_free(checksum);
    GThread* readingThread = g_thread_new(NULL, (GThreadFunc) client_reading_loop, &app_io);
    GThread* writingOutThread = g_thread_new(NULL, (GThreadFunc) client_writing_shell_out_loop, &app_io);
    g_thread_join(readingThread);
    g_thread_join(writingOutThread);
    g_io_channel_unref(stdout_channel);
    g_io_channel_unref(stdin_channel);
    g_thread_unref(readingThread);
    g_thread_unref(writingOutThread);
    //connection_close(&connection);
    g_object_unref(client);
    return 0;
}
