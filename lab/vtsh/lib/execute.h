#pragma once

#include "background.h" 

typedef struct {
    char** args;
    int background;
} ChildProcessData;

int execute_builtin_command(char** args, const char* initial_directory);
int execute_external_command(char** args, int background, BackgroundProcesses* bg_procs);
