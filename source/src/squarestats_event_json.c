/* squarestats_event_json.c */

#include <stdlib.h>
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
        cJSON_AddNumberToObject(root, "time", (double)time);
        cJSON_AddBoolToObject(root, "gib", gib);
        cJSON_AddBoolToObject(root, "suicide", suicide);
        
        const char* json = cJSON_Print(root);
        cJSON_Delete(root);
        
        return json;
}