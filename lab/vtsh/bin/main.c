#include <errno.h>
#include <fcntl.h>
#include <signal.h>
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
#define MAX_BACKGROUND_PROCESSES 100
#define STACK_SIZE (64UL * 1024)
#define MICROSECONDS_PER_SECOND 1000000.0

typedef struct {
  pid_t pids[MAX_BACKGROUND_PROCESSES];
  void* stacks[MAX_BACKGROUND_PROCESSES];
  int count;
} BackgroundProcesses;

typedef struct {
  char** args;
  int background;
} ChildProcessData;

void redirect_std_to_null() {
  int null_fd = open("/dev/null", O_RDONLY);
  if (null_fd != -1) {
    dup2(null_fd, STDIN_FILENO);
    close(null_fd);
  }
  null_fd = open("/dev/null", O_WRONLY);
  if (null_fd != -1) {
    dup2(null_fd, STDOUT_FILENO);
    dup2(null_fd, STDERR_FILENO);
    close(null_fd);
  }
}

static int child_exec(void* arg) {
  ChildProcessData* data = (ChildProcessData*)arg;

  if (data->background) {
    redirect_std_to_null();
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

int parse_command(char* input, char** args) {
  int arg_count = 0;
  char* saveptr = NULL;
  char* token = strtok_r(input, " \t", &saveptr);
  int background = 0;

  while (token != NULL && arg_count < MAX_ARGS - 1) {
    args[arg_count++] = token;
    token = strtok_r(NULL, " \t", &saveptr);
  }
  args[arg_count] = NULL;

  if (arg_count > 0 && strcmp(args[arg_count - 1], "&") == 0) {
    args[arg_count - 1] = NULL;
    background = 1;
  }

  return background;
}

void check_background_processes(BackgroundProcesses* bg_procs) {
  int status = 0;
  pid_t pid = 0;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    printf("[%d] завершен\n", pid);

    for (int i = 0; i < bg_procs->count; i++) {
      if (bg_procs->pids[i] == pid) {
        free(bg_procs->stacks[i]);
        bg_procs->pids[i] = bg_procs->pids[bg_procs->count - 1];
        bg_procs->stacks[i] = bg_procs->stacks[bg_procs->count - 1];
        bg_procs->count--;
        break;
      }
    }
  }
}

int execute_builtin(char** args, const char* initial_directory) {
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

void handle_exit(BackgroundProcesses* bg_procs) {
  while (bg_procs->count > 0) {
    printf("Ожидание завершения фоновых процессов...\n");
    usleep(MICROSECONDS_PER_SECOND);
    check_background_processes(bg_procs);
  }
}

int read_line(char* buffer, int max_size) {
  int idx = 0;
  char sym = '0';
  while (idx < max_size - 1) {
    size_t n_bytes = read(STDIN_FILENO, &sym, 1);
    if (n_bytes <= 0) {
      return -1;
    }
    if (sym == '\n') {
      break;
    }
    buffer[idx++] = sym;
  }
  buffer[idx] = '\0';
  return idx;
}

int main() {
  char input[MAX_INPUT_SIZE];
  char* args[MAX_ARGS];
  BackgroundProcesses bg_procs = {.count = 0};
  char initial_directory[MAX_PATH_SIZE];

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

    int len = read_line(input, MAX_INPUT_SIZE);
    if (len < 0) {
      break;
    }

    int background = parse_command(input, args);

    if (args[0] == NULL) {
      continue;
    }

    if (strcmp(args[0], "exit") == 0) {
      handle_exit(&bg_procs);
      break;
    }

    struct timeval start_time;
    struct timeval end_time;

    if (!background) {
      gettimeofday(&start_time, NULL);
    }

    int command_executed = 0;
    if (execute_builtin(args, initial_directory)) {
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
