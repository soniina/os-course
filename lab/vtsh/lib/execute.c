#define _GNU_SOURCE

#include "execute.h"

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "background.h"
#include "vtsh.h"

static int child_exec(void* arg) {
  ChildProcessData* data = (ChildProcessData*)arg;

  if (data->background) {
    redirect_background_std();
  }

  execvp(data->args[0], data->args);

  if (errno == ENOENT) {
    printf("Command not found\n");
  } else {
    perror(data->args[0]);
  }
  if (fflush(stdout) != 0) {
    perror("fflush failed");
  }
  _exit(EXIT_FAILURE);
}

int execute_builtin_command(char** args, const char* initial_directory) {
  if (strcmp(args[0], "cd") == 0) {
    const char* target_dir = args[1] != NULL ? args[1] : initial_directory;
    if (chdir(target_dir) != 0) {
      perror("cd");
    }
    return 1;
  }
  return 0;
}

int execute_external_command(
    char** args, int background, BackgroundProcesses* bg_procs
) {
  char* stack = malloc(STACK_SIZE);
  if (stack == NULL) {
    perror("malloc failed");
    return -1;
  }

  ChildProcessData data = {.args = args, .background = background};
  pid_t pid = clone(child_exec, stack + STACK_SIZE, SIGCHLD, &data);

  if (pid == -1) {
    perror("fork failed");
    return -1;
  }

  if (background) {
    printf("[%d] запущен в фоне\n", pid);
    if (bg_procs->count < MAX_BACKGROUND_PROCESSES) {
      bg_procs->pids[bg_procs->count] = pid;
      bg_procs->stacks[bg_procs->count++] = stack;
    } else {
      printf("Too many background processes\n");
      free(stack);
    }
  } else {
    int status = 0;
    waitpid(pid, &status, 0);
    free(stack);
  }

  return pid;
}
