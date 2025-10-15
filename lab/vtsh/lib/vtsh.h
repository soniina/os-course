#pragma once

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_PATH_SIZE 1024
#define STACK_SIZE (64UL * 1024)
#define MAX_BACKGROUND_PROCESSES 100
#define MICROSECONDS_PER_SECOND 1000000.0

const char* vtsh_prompt();
int parse_command(char* input, char** args);
