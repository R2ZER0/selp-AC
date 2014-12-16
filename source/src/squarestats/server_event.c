/* server_event.c */

#include <stdlib.h>
#include <stdio.h>
#include "event_json.h"
#include "server_zmq.h"

/* Called when a player kills another (or themselves). */
void on_event_kill(const char* killer, const char* victim, const char* weapon,
                   unsigned int time, int gib, int suicide)
{        
        const char* json = event_kill_json(killer, victim, weapon, time, gib,
                                           suicide);
        
        send_message(json);
        
        free((void*) json);
        json = NULL;
}