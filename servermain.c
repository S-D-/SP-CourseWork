#include <stdio.h>
#include "networking/server.h"
#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>

#ifdef __WIN32__
#include <windows.h>
#elif __UNIX__
#include <signal.h>
#endif

GMainLoop *loop;

#ifdef __WIN32__
BOOL WINAPI onInterupt(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        printf("(WIN) Exiting...\n");
        g_main_loop_quit (loop);
    }
    return TRUE;
}
#elif __UNIX__
void onInterupt(int sig) {
    printf("(UNIX) Exiting...\n");
    g_main_loop_quit (loop);
}
#endif

int main(void)
{
    printf("HELLO!!!\n");
    g_type_init();
#ifdef __WIN32__
    if (!SetConsoleCtrlHandler(onInterupt, TRUE)) {
        printf("\nERROR: Could not set control handler"); 
        return EXIT_FAILURE;
    }
#elif __UNIX__
    if (signal(SIGINT, onInterupt) == SIG_ERR){
        printf("\nERROR: Could not set control handler");
        return EXIT_FAILURE;
    }
#endif
    Server nw = {
        3336,
        false,
        false
    };
    start(&nw);
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    return 0;
}

