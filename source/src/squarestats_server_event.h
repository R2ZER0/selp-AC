/* squarestats_server_event.h */
#ifndef _SQUARESTATS_SERVER_EVENT_H_
#define _SQUARESTATS_SERVER_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

void on_event_kill(const char* killer, const char* victim, const char* weapon,
                   unsigned int time, int gib, int suicide);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SQUARESTATS_SERVER_EVENT_H_ */