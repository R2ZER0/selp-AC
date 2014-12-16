/* server_zmq.c */

#include <stdlib.h>
#include <zmq.h>

void* context = NULL;
void* pub = NULL;

void init_zmq(void)
{
        context = zmq_context_new();
}

void send_message(const char* message)
{
    
}