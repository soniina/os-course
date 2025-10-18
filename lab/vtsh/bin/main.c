#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "background.h"
#include "execute.h"
#include "vtsh.h"

int main() {
  char input[MAX_INPUT_SIZE];
  char* args[MAX_ARGS];
  BackgroundProcesses bg_procs = {.count = 0};
  char initial_directory[MAX_PATH_SIZE];

  if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
    perror("setvbuf failed");
    return 1;
  }

  if (getcwd(initial_directory, sizeof(initial_directory)) == NULL) {
    perror("getcwd failed");
    return 1;
  }

  while (1) {
    check_background_processes(&bg_procs);

    printf("%s", vtsh_prompt());
    if (fflush(stdout) != 0) {
      perror("fflush failed");
      continue;
    }

    if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
      handle_exit(&bg_procs);
      break;
    }

    int background = parse_command(input, args);

    if (args[0] == NULL) {
      continue;
    }

    if (strcmp(args[0], "exit") == 0) {
      printf("\n");
      break;
    }

    struct timeval start_time;
    struct timeval end_time;

    if (!background) {
      gettimeofday(&start_time, NULL);
    }

    int command_executed = 0;
    if (execute_builtin_command(args, initial_directory)) {
      command_executed = 1;
    } else {
      pid_t pid = execute_external_command(args, background, &bg_procs);
      command_executed = (pid != -1);
    }

    if (!background && command_executed) {
      gettimeofday(&end_time, NULL);
      double elapsed_time = (double)(end_time.tv_sec - start_time.tv_sec) +
                            (double)(end_time.tv_usec - start_time.tv_usec) /
                                MICROSECONDS_PER_SECOND;
    }
  }

  return 0;
}
