#include <stdio.h>
#include "server/server.h"
#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include "utils/usersinfo.h"
#include "utils/usersxmlparser.h"
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>


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

void print_entry(gpointer key, gpointer value, gpointer user_data)
{
    printf("key:%s\n",key);
    printf(((UserInfo*)value)->name);
    printf(((UserInfo*)value)->password);
}

void test()
{
    FILE* keypair_file = fopen("keypair.pem", "r");
    printf("reading keys...\n");
    RSA* keypair = PEM_read_RSAPrivateKey(keypair_file, NULL, NULL, NULL);
    if (keypair == NULL) {
        printf("error reading\n");
        ERR_print_errors_fp(stderr);
        fclose(keypair_file);
        return;
    }
    fclose(keypair_file);
    FILE* pubkey_file = fopen("pubkey.pem", "w");
    PEM_write_RSAPublicKey(pubkey_file, keypair);
    fclose(pubkey_file);
    pubkey_file = fopen("pubkey.pem", "r");
    RSA* pubkey = PEM_read_RSAPublicKey(pubkey_file, NULL, NULL, NULL);
    fclose(pubkey_file);
    printf("keys read\n");
    char* encrypt = (char*)malloc(RSA_size(keypair));
    char* err = (char*)malloc(130);
    char* msg = "hello!";
    int enc_size;
    printf("encrypting...\n");
    if ((enc_size = RSA_public_encrypt(6, (unsigned char*)msg, (unsigned char*)encrypt,
        pubkey, RSA_PKCS1_OAEP_PADDING)) < 0) {
        printf("encrypting failed\n");
        ERR_load_crypto_strings();
        ERR_error_string(ERR_get_error(), err);
        fprintf(stdin, "Error encrypting message: %s\n", err);
        free(encrypt);
        free(err);
        return;
    }
    printf("encrypted\n");
    printf("Encr:%.*s\n", enc_size, encrypt);
    
    // Decrypt it
    printf("RSA SIZE Keypair:%u\n",RSA_size(keypair));
    char* decrypt = (char*)malloc(RSA_size(keypair));
    int decr_res;
    if ((decr_res = RSA_private_decrypt(enc_size, (unsigned char*)encrypt, (unsigned char*)decrypt,
        keypair, RSA_PKCS1_OAEP_PADDING)) < 0) {
        ERR_load_crypto_strings();
        ERR_error_string(ERR_get_error(), err);
        fprintf(stdin, "Error decrypting message: %s\n", err);
        free(decrypt);
        free(err);
        return;
    }
    printf("decr_size=%i\n", decr_res);
    printf("Decr:%.*s\n", decr_res, decrypt);
}

int main(void)
{
    g_type_init();
    USERS_INFO = parse_users_info("users.xml");
    if (USERS_INFO) {
        g_hash_table_foreach(USERS_INFO, print_entry, NULL);
    }
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
    printf("freeing USERS_INFO...\n");
    users_info_free(USERS_INFO);
    return 0;
}

