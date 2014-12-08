/* squarestats_event_json.h */
#ifndef _SQUARESTATS_EVENT_JSON_H_
#define _SQUARESTATS_EVENT_JSON_H_

const char* event_kill_json(const char* killer, const char* victim,
                            const char* weapon, unsigned int time, int gib,
                            int suicide);

#endif /* _SQUARESTATS_EVENT_JSON_H_ */