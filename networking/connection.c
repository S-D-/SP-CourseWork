#include "connection.h"
#include <stdlib.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <string.h>

#define KEY_LENGTH  2048
#define PUB_EXP     3
#define PRINT_KEYS
#define WRITE_TO_FILE

char* encrypt_msg(gchar* message, Connection* conn, size_t* encrypt_len){
        size_t pri_len;            // Length of private key
        size_t pub_len;            // Length of public key
        char   *pri_key;           // Private key
        char   *pub_key;           // Public key
        char*   msg = message;    // Message to encrypt
        char   *encrypt = NULL;    // Encrypted message
        char   *err;               // Buffer for any error messages

        // Generate key pair
        printf("Generating RSA (%d bits) keypair...\n", KEY_LENGTH);
        fflush(stdout);
        RSA *keypair = RSA_generate_key(KEY_LENGTH, PUB_EXP, NULL, NULL);
        printf("KEYPAIR:%s\n", *keypair);
        conn->keypair = keypair;

        // To get the C-string PEM form:
        BIO *pri = BIO_new(BIO_s_mem());
        BIO *pub = BIO_new(BIO_s_mem());

        PEM_write_bio_RSAPrivateKey(pri, keypair, NULL, NULL, 0, NULL, NULL);
        PEM_write_bio_RSAPublicKey(pub, keypair);

        pri_len = BIO_pending(pri);
        pub_len = BIO_pending(pub);

        pri_key = (char*)malloc(pri_len + 1);
        pub_key = (char*)malloc(pub_len + 1);

        BIO_read(pri, pri_key, pri_len);
        BIO_read(pub, pub_key, pub_len);

        pri_key[pri_len] = '\0';
        pub_key[pub_len] = '\0';

        // Encrypt the message
        printf("RSA SIZE Keypair:%u\n",RSA_size(keypair));
        encrypt = (char*)malloc(RSA_size(keypair));
        err = (char*)malloc(130);
        //int encrypt_len;
        if ((*encrypt_len = RSA_public_encrypt(strlen(msg) + 1, (unsigned char*)msg, (unsigned char*)encrypt,
            keypair, RSA_PKCS1_OAEP_PADDING)) == -1) {
            ERR_load_crypto_strings();
            ERR_error_string(ERR_get_error(), err);
            fprintf(stderr, "Error encrypting message: %s\n", err);
            free(encrypt);
            free(err);
            return NULL;
        }
        printf("Encrypt length:%u\n", *encrypt_len);
        printf("Encrypt message:%u\n", *encrypt);
        return encrypt;
}

char* decrypt_msg(int encrypt_len, gchar* encrypt, RSA* keypair){
    char   *decrypt = NULL;    // Decrypted message
    char   *err = (char*)malloc(130);
    // Decrypt it
    printf("RSA SIZE Keypair:%u\n",RSA_size(keypair));
    decrypt = malloc(RSA_size(keypair));
    if (RSA_private_decrypt(encrypt_len, (unsigned char*)encrypt, (unsigned char*)decrypt,
        keypair, RSA_PKCS1_OAEP_PADDING) == -1) {
        ERR_load_crypto_strings();
        ERR_error_string(ERR_get_error(), err);
        fprintf(stderr, "Error decrypting message: %s\n", err);
        free(decrypt);
        free(err);
        return NULL;
    }
    printf("Decrypted message: %s\n", decrypt);
    return decrypt;
}

/**
 * Sends message prepended with 1 byte of the message size information.
 * If message size >= 255 then message is sent by chunks of 254 chars with size info equal to 255.
 * @brief connetion_send_message
 * @param connection
 * @param message
 * @param size
 */
void connection_send_message(Connection* connection, char* message, size_t size)
{
    message = encrypt_msg(message, connection, &size);
    RSA* keypair = connection->keypair;
    printf("KEYPAIR:%s\n", *keypair);
    GError * error = NULL;
    GOutputStream * ostream = g_io_stream_get_output_stream (G_IO_STREAM (connection->gSockConnection));
    size_t tmpSize = size;
    gsize bytesWritten;
    while (tmpSize >= 255) {
        unsigned char s = 255u;
        g_output_stream_write_all(ostream, &s,1, &bytesWritten, NULL,&error);
        if (error != NULL)
        {
            g_error (error->message);
            g_error_free (error);
            return;
        }
        g_output_stream_write_all(ostream,&message[size - tmpSize],254,
                &bytesWritten,NULL,&error);
        if (error != NULL)
        {
            g_error (error->message);
            g_error_free (error);
            return;
        }
        tmpSize -= 254;
    }
    g_output_stream_write_all(ostream, &tmpSize, 1, &bytesWritten, NULL, &error);
    if (error != NULL)
    {
        g_error (error->message);
        g_error_free (error);
        return;
    }
    g_output_stream_write_all(ostream, &message[size - tmpSize], tmpSize,
            &bytesWritten, NULL, &error);
    if (error != NULL)
    {
        g_error (error->message);
        g_error_free (error);
        return;
    }
}

char* connection_read_message(Connection* connection, size_t* readSize)
{
    GInputStream * istream = g_io_stream_get_input_stream (G_IO_STREAM(connection->gSockConnection));
    gsize bytesRead;
    unsigned char msgSize;
    g_input_stream_read_all(istream,
                            &msgSize,
                            1,
                            &bytesRead,
                            NULL,
                            NULL);
    size_t resTotalSize = 0;
    gchar* message = NULL;
    while (msgSize >= 255) {
        resTotalSize += 254;
        message = realloc(message, resTotalSize);
        g_input_stream_read_all(istream,
                                &message[resTotalSize - 254],
                                254,
                                &bytesRead,
                                NULL,
                                NULL);
        g_input_stream_read_all(istream,
                                &msgSize,
                                1,
                                &bytesRead,
                                NULL,
                                NULL);
    }
    resTotalSize += msgSize;
    message = realloc(message, resTotalSize);
    g_input_stream_read_all(istream,
                            &message[resTotalSize - msgSize],
            msgSize,
            &bytesRead,
            NULL,
            NULL);
    *readSize = resTotalSize;
    message = decrypt_msg((int)msgSize, message, connection->keypair);
    return message;
}

void connection_close(Connection *connection)
{
    g_object_unref(connection->gSockConnection);
}
