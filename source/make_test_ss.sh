#!/bin/bash
clang -o t/ss_event_json.t -I./src src/squarestats/event_json_test.c src/squarestats/event_json.c src/squarestats/cJSON.c -lcheck -lm -g $@ 
