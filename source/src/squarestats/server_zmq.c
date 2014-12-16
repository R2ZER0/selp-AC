/* server_zmq.c */

#include <stdlib.h>
#include <string.h>
#include <zmq.h>

void init_zmq(void);
void deinit_zmq(void);

void* context = NULL;
void* pub = NULL;

/* Create our socket */
void init_zmq(void)
{
        context = zmq_context_new();
        pub = zmq_socket(context, ZMQ_PUB);
        zmq_bind(pub, "tcp://*:28765");
        atexit(deinit_zmq);
}

/* Clean up */
void deinit_zmq(void) {
        zmq_close(pub);
        pub = NULL;
        
        zmq_ctx_destroy(context);
        context = NULL;
}

/* This file's public function:
 * Send the given string as a simple one-part message */
void send_message(const char* message)
{
        if(!pub) { init_zmq(); }
        
        /* Send the message with DONTWAIT, we cannot afford to block the server,
         * so if there is not enough space in the (rather large) zmq socket
         * buffer, this is not NASA, missing one bit of data won't hurt. */
        zmq_send(pub, message, strlen(message), ZMQ_DONTWAIT);
}