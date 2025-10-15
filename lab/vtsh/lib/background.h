#pragma once

#include <sys/wait.h>
#include "vtsh.h"

typedef struct {
  pid_t pids[MAX_BACKGROUND_PROCESSES];
  void* stacks[MAX_BACKGROUND_PROCESSES];
  int count;
} BackgroundProcesses;

void redirect_background_std(void);
void remove_background_process(BackgroundProcesses* bg_procs, pid_t pid);
void check_background_processes(BackgroundProcesses* bg_procs);
void handle_exit(BackgroundProcesses* bg_procs);
