#ifndef CONNECTION_H
#define CONNECTION_H
#include <glib.h>
#include <gio/gio.h>

typedef struct connection {
    GSocketConnection *gSockConnection;
} Connection;

void connection_send_message(Connection *connection, const char *message, size_t size);
char* connection_read_message(Connection* connection, size_t *readSize);
void connection_close(Connection* connection);

#endif //CONNECTION_H
