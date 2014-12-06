#include "connection.h"
#include <stdlib.h>

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
    return message;
}

void connection_close(Connection *connection)
{
    g_object_unref(connection->gSockConnection);
}
