#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BASE_DECIMAL 10
#define WORD_LENGTH 9
#define DEFAULT_OUTPUT_FILENAME "join_result.txt"
#define BUFFER_SIZE 32

typedef struct {
  char** file1;
  char** file2;
  char** output;
} file_args_t;

typedef struct {
  char* file1;
  char* file2;
  char* output;
  long iterations;
} thread_args_t;

typedef struct {
  int id;
  char word[WORD_LENGTH];
} row_t;

row_t* load_table(const char* filename, int* count) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    perror(filename);
    return NULL;
  }

  char buffer[BUFFER_SIZE];
  if (fgets(buffer, sizeof(buffer), file) == NULL) {
    perror("fgets failed");
    (void)fclose(file);
    return NULL;
  }

  char* endptr = 0;
  long num_rows = strtol(buffer, &endptr, BASE_DECIMAL);
  if (endptr == buffer || num_rows <= 0) {
    (void)fprintf(stderr, "Invalid number of rows in file: %s\n", filename);
    (void)fclose(file);
    return NULL;
  }
  *count = (int)num_rows;

  row_t* table = malloc(*count * sizeof(row_t));
  if (table == NULL) {
    perror("malloc failed");
    (void)fclose(file);
    return NULL;
  }

  for (int i = 0; i < *count; i++) {
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
      perror("fgets failed");
      free(table);
      (void)fclose(file);
      return NULL;
    }

    char* saveptr = NULL;
    char* token1 = strtok_r(buffer, " \t\n", &saveptr);
    char* token2 = strtok_r(NULL, " \t\n", &saveptr);
    if (token1 == NULL || token2 == NULL) {
      (void)fprintf(
          stderr, "Invalid format in file: %s (line %d)\n", filename, i + 1
      );
      free(table);
      (void)fclose(file);
      return NULL;
    }

    table[i].id = (int)strtol(token1, &endptr, BASE_DECIMAL);
    if (endptr == token1) {
      (void
      )fprintf(stderr, "Invalid id in file: %s (line %d)\n", filename, i + 1);
      free(table);
      (void)fclose(file);
      return NULL;
    }

    strncpy(table[i].word, token2, WORD_LENGTH - 1);
    table[i].word[WORD_LENGTH - 1] = '\0';
  }

  (void)fclose(file);
  return table;
}

void nested_loop_join(
    row_t* table1, int count1, row_t* table2, int count2, FILE* output
) {
  int total_rows = 0;
  for (int i = 0; i < count1; i++) {
    for (int j = 0; j < count2; j++) {
      if (table1[i].id == table2[j].id) {
        total_rows++;
      }
    }
  }

  (void)fprintf(output, "%d\n", total_rows);
  for (int i = 0; i < count1; i++) {
    for (int j = 0; j < count2; j++) {
      if (table1[i].id == table2[j].id) {
        (void)fprintf(
            output, "%d %s %s\n", table1[i].id, table1[i].word, table2[j].word
        );
      }
    }
  }
}

static int parse_arguments(
    int argc, char* argv[], file_args_t* files, long* iterations
) {
  for (int i = 1; i < argc; i++) {
    char* key = argv[i++];
    if (i >= argc) {
      (void)fprintf(stderr, "Missing value for --%s\n", key);
      return -1;
    }
    char* value = argv[i];

    if (strcmp(key, "--file1") == 0) {
      *(files->file1) = value;
    } else if (strcmp(key, "--file2") == 0) {
      *(files->file2) = value;
    } else if (strcmp(key, "--output") == 0) {
      *(files->output) = value;
    } else if (strcmp(key, "--iterations") == 0) {
      char* endptr = NULL;
      long tmp_iterations = strtol(value, &endptr, BASE_DECIMAL);
      if (*endptr != '\0' || tmp_iterations <= 0) {
        (void)fprintf(stderr, "Invalid iterations: %s\n", value);
        return -1;
      }
      *iterations = tmp_iterations;
    } else {
      (void)fprintf(stderr, "Unknown parameter: %s\n", key);
      return -1;
    }
  }

  if (!*(files->file1) || !*(files->file2)) {
    return -1;
  }
  return 0;
}

void* thread_work(void* arg) {
  thread_args_t* data = (thread_args_t*)arg;

  for (int iter = 0; iter < data->iterations; iter++) {
    int count1 = 0;
    int count2 = 0;
    row_t* table1 = load_table(data->file1, &count1);
    if (!table1) {
      pthread_exit(NULL);
    }
    row_t* table2 = load_table(data->file2, &count2);
    if (!table2) {
      free(table1);
      pthread_exit(NULL);
    }

    FILE* file = fopen(data->output, "w");
    if (file == NULL) {
      perror("Failed to open output file");
      free(table1);
      free(table2);
      pthread_exit(NULL);
    }
    nested_loop_join(table1, count1, table2, count2, file);
    (void)fclose(file);

    free(table1);
    free(table2);
  }

  pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
  char* file1 = NULL;
  char* file2 = NULL;
  char* output = DEFAULT_OUTPUT_FILENAME;
  file_args_t files = {&file1, &file2, &output};
  long iterations = 1;

  if (argc <= 2 || parse_arguments(argc, argv, &files, &iterations) != 0) {
    (void)fprintf(
        stderr,
        "Usage: %s --file1 <path> --file2 <path> [--output <path>] "
        "[--iterations <count>]\n",
        argv[0]
    );
    return 1;
  }

  long num_threads = sysconf(_SC_NPROCESSORS_ONLN);
  pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
  thread_args_t* thread_args = malloc(num_threads * sizeof(thread_args_t));

  if (threads == NULL || thread_args == NULL) {
    perror("malloc failed");
    return 1;
  }

  for (int i = 0; i < num_threads; i++) {
    thread_args[i].file1 = file1;
    thread_args[i].file2 = file2;
    thread_args[i].output = output;
    thread_args[i].iterations = iterations;

    if (pthread_create(&threads[i], NULL, thread_work, &thread_args[i]) != 0) {
      perror("pthread_create failed");
      free(threads);
      free(thread_args);
      return 1;
    }
  }

  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  free(threads);
  free(thread_args);

  return 0;
}
