#!/bin/bash
clang -o run_test_ss -I./src src/squarestats_event_json_test.c src/squarestats_event_json.c src/cJSON.c -lcheck -lm -g
