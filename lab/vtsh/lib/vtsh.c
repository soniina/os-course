#include "vtsh.h"

#include <stdio.h>
#include <string.h>

const char* vtsh_prompt() {
  return "vtsh> ";
}

int parse_command(char* input, char** args) {
  input[strcspn(input, "\n")] = '\0';

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
