/* squarestats_server_event.c */

#include "cJSON.h"

/* Called when a player kills another (or themselves). */
void on_event_kill(const char* killer, const char* victim, const char* weapon,
                   unsigned int time, int gib, int suicide)
{
        /* TODO */
}