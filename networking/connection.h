#ifndef CONNECTION_H
#define CONNECTION_H
#include <glib.h>
#include <gio/gio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

typedef struct connection {
    GSocketConnection *gSockConnection;
    RSA *keypair;
} Connection;

void connection_send_message(Connection *connection, char* message, size_t size);
char* connection_read_message(Connection* connection, size_t *readSize);
void connection_close(Connection* connection);

#endif //CONNECTION_H
