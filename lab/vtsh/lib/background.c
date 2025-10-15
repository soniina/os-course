#include "background.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void redirect_background_std() {
  int null_fd = open("/dev/null", O_RDONLY);
  if (null_fd != -1) {
    dup2(null_fd, STDIN_FILENO);
    close(null_fd);
  }
  null_fd = open("/dev/null", O_WRONLY);
  if (null_fd != -1) {
    dup2(null_fd, STDOUT_FILENO);  // мб в файл
    dup2(null_fd, STDERR_FILENO);  // мб в файл
    close(null_fd);
  }
}

void remove_background_process(BackgroundProcesses* bg_procs, pid_t pid) {
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

void check_background_processes(BackgroundProcesses* bg_procs) {
  int status = 0;
  pid_t pid = 0;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    printf("[%d] завершен\n", pid);
    remove_background_process(bg_procs, pid);
  }
}

void handle_exit(BackgroundProcesses* bg_procs) {
  while (bg_procs->count > 0) {
    int status = 0;
    pid_t pid = waitpid(-1, &status, 0);
    if (pid > 0) {
      remove_background_process(bg_procs, pid);
    } else {
      break;
    }
  }
}
