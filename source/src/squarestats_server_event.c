/* squarestats_server_event.c */

#include <stdlib.h>
#include <stdio.h>
#include "cJSON.h"

/* Generate the json representation of a kill event */
const char* event_kill_json(const char* killer, const char* victim,
                            const char* weapon, unsigned int time, int gib,
                            int suicide)
{
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "killer", killer);
        cJSON_AddStringToObject(root, "victim", victim);
        cJSON_AddStringToObject(root, "weapon", weapon);
        /* We recieve the time in milliseconds */
        cJSON_AddNumberToObject(root, "time", ((double) time / 1000.0));
        cJSON_AddBoolToObject(root, "gib", gib);
        cJSON_AddBoolToObject(root, "suicide", suicide);
        
        const char* json = cJSON_Print(root);
        cJSON_Delete(root);
        
        return json;
}

/* Called when a player kills another (or themselves). */
void on_event_kill(const char* killer, const char* victim, const char* weapon,
                   unsigned int time, int gib, int suicide)
{        
        const char* json = event_kill_json(killer, victim, weapon, time, gib,
                                           suicide);
        
        printf("%s", json);
        
        free((void*) json);
        json = NULL;
}