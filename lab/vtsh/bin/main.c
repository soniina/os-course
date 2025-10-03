#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "vtsh.h"

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_PATH_SIZE 1024
#define MICROSECONDS_PER_SECOND 1000000.0

void parse_command(char* input, char** args) {
  int arg_count = 0;
  char* saveptr = NULL;
  char* token = strtok_r(input, " \t", &saveptr);

  while (token != NULL && arg_count < MAX_ARGS - 1) {
    args[arg_count++] = token;
    token = strtok_r(NULL, " \t", &saveptr);
  }
  args[arg_count] = NULL;
}

int execute_builtin(char** args, const char* initial_directory) {
  if (strcmp(args[0], "cd") == 0) {
    if (args[1] == NULL) {
      if (chdir(initial_directory) != 0) {
        perror("cd");
      }
    } else {
      if (chdir(args[1]) != 0) {
        perror("cd");
      }
    }
    return 1;
  }
  return 0;
}

int main() {
  char input[MAX_INPUT_SIZE];
  char* args[MAX_ARGS];

  char initial_directory[MAX_PATH_SIZE];
  if (getcwd(initial_directory, sizeof(initial_directory)) == NULL) {
    perror("getcwd failed");
    return 1;
  }

  while (1) {
    printf("%s", vtsh_prompt());
    if (fflush(stdout) != 0) {
      perror("fflush failed");
      continue;
    }

    if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
      printf("\n");
      break;
    }

    input[strcspn(input, "\n")] = '\0';

    parse_command(input, args);

    if (args[0] == NULL) {
      continue;
    }

    if (strcmp(args[0], "exit") == 0) {
      break;
    }

    if (execute_builtin(args, initial_directory)) {
      continue;
    }

    struct timeval start_time;
    struct timeval end_time;
    gettimeofday(&start_time, NULL);

    pid_t pid = fork();

    if (pid == -1) {
      perror("fork failed");
      continue;
    }

    if (pid == 0) {
      execvp(args[0], args);

      if (errno == ENOENT) {
        printf("Command not found\n");
      } else {
        perror(args[0]);
      }

      if (fflush(stdout) != 0) {
        perror("fflush failed");
        continue;
      }

      _exit(EXIT_FAILURE);
    }

    int status = -1;
    waitpid(pid, &status, 0);

    gettimeofday(&end_time, NULL);

    double elapsed_time = (double)(end_time.tv_sec - start_time.tv_sec) +
                          (double)(end_time.tv_usec - start_time.tv_usec) /
                              MICROSECONDS_PER_SECOND;
  }

  return 0;
}
