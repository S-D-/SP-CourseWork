#ifndef SERVER_H
#define SERVER_H
#include <stdbool.h>
typedef struct server {
    int port;
    volatile bool finished;
    volatile bool canceled;
} Server;
void start(Server* server);
void cancel(Server* server);
void waitForFinished(Server server);
#endif //SERVER_H
